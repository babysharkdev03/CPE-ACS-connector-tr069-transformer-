#include "tr069/ConnectionRequestServer.hpp"
#include "tr069/Logger.hpp"
#include "tr069/Md5.hpp"

#include <arpa/inet.h>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <netinet/in.h>
#include <random>
#include <sstream>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace tr069 {

ConnectionRequestServer::ConnectionRequestServer(ConnectionRequestConfig config,
                                                 std::function<void()> onRequest)
    : config_(std::move(config)), onRequest_(std::move(onRequest)) {
    std::random_device random;
    nonce_ = md5Hex(config_.realm + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                    std::to_string(random()));
}

ConnectionRequestServer::~ConnectionRequestServer() { stop(); }

bool ConnectionRequestServer::start() {
    if (running_) return true;
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) return false;
    int reuse = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(config_.port));
    if (inet_pton(AF_INET, config_.bindAddress.c_str(), &address.sin_addr) != 1) {
        close(listenFd_); listenFd_ = -1; return false;
    }
    if (bind(listenFd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(listenFd_, 4) != 0) {
        close(listenFd_); listenFd_ = -1; return false;
    }
    running_ = true;
    thread_ = std::thread(&ConnectionRequestServer::serve, this);
    Logger::info("Connection Request server listening on " + config_.bindAddress + ":" +
                 std::to_string(config_.port));
    return true;
}

void ConnectionRequestServer::stop() {
    running_ = false;
    if (listenFd_ >= 0) shutdown(listenFd_, SHUT_RDWR);
    if (thread_.joinable()) thread_.join();
    if (listenFd_ >= 0) close(listenFd_);
    listenFd_ = -1;
}

std::string ConnectionRequestServer::headerValue(const std::string& request, const std::string& name) {
    size_t lineStart = request.find("\r\n");
    if (lineStart == std::string::npos) return {};
    lineStart += 2;
    while (lineStart < request.size()) {
        const size_t lineEnd = request.find("\r\n", lineStart);
        if (lineEnd == std::string::npos || lineEnd == lineStart) break;
        const size_t colon = request.find(':', lineStart);
        if (colon != std::string::npos && colon < lineEnd) {
            const std::string headerName = request.substr(lineStart, colon - lineStart);
            if (strcasecmp(headerName.c_str(), name.c_str()) == 0) {
                size_t valueStart = colon + 1;
                while (valueStart < lineEnd &&
                       (request[valueStart] == ' ' || request[valueStart] == '\t')) {
                    ++valueStart;
                }
                return request.substr(valueStart, lineEnd - valueStart);
            }
        }
        lineStart = lineEnd + 2;
    }
    return {};
}

std::string ConnectionRequestServer::digestValue(const std::string& auth, const std::string& key) {
    size_t pos = auth.find(key + "=");
    if (pos == std::string::npos) return {};
    pos += key.size() + 1;
    if (pos < auth.size() && auth[pos] == '\"') {
        const size_t end = auth.find('\"', ++pos);
        return auth.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    }
    const size_t end = auth.find(',', pos);
    std::string value = auth.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    while (!value.empty() && value.back() == ' ') value.pop_back();
    return value;
}

bool ConnectionRequestServer::authorized(const std::string& request,
                                         const std::string& method) const {
    std::string authMode = config_.auth;
    std::transform(authMode.begin(), authMode.end(), authMode.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (authMode == "none") {
        return true;
    }
    const std::string auth = headerValue(request, "Authorization");
    if (auth.rfind("Digest ", 0) != 0) {
        Logger::warn("[CONNECTION_REQUEST] digest authorization missing");
        return false;
    }
    const std::string username = digestValue(auth, "username");
    const std::string realm = digestValue(auth, "realm");
    const std::string nonce = digestValue(auth, "nonce");
    const std::string uri = digestValue(auth, "uri");
    const std::string response = digestValue(auth, "response");
    const std::string qop = digestValue(auth, "qop");
    const std::string nc = digestValue(auth, "nc");
    const std::string cnonce = digestValue(auth, "cnonce");
    if (username != config_.username || realm != config_.realm || nonce != nonce_ || uri.empty()) {
        Logger::warn("[CONNECTION_REQUEST] digest authorization rejected username=" +
                     username + " realm=" + realm + " uri=" +
                     (uri.empty() ? "<empty>" : uri));
        return false;
    }
    const std::string ha1 = md5Hex(username + ":" + realm + ":" + config_.password);
    const std::string ha2 = md5Hex(method + ":" + uri);
    const std::string expected = qop.empty()
        ? md5Hex(ha1 + ":" + nonce + ":" + ha2)
        : md5Hex(ha1 + ":" + nonce + ":" + nc + ":" + cnonce + ":" + qop + ":" + ha2);
    const bool ok = response == expected;
    Logger::info(std::string("[CONNECTION_REQUEST] digest authorization ") +
                 (ok ? "accepted" : "rejected") + " username=" + username +
                 " uri=" + uri);
    return ok;
}

std::string ConnectionRequestServer::challenge() const {
    return "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Digest realm=\"" + config_.realm +
           "\", nonce=\"" + nonce_ + "\", algorithm=MD5, qop=\"auth\"\r\n"
           "Content-Length: 0\r\nConnection: close\r\n\r\n";
}

namespace {
std::string requestLine(const std::string& request) {
    const size_t end = request.find("\r\n");
    return request.substr(0, end == std::string::npos ? request.size() : end);
}

std::string requestMethod(const std::string& request) {
    const std::string line = requestLine(request);
    const size_t space = line.find(' ');
    return space == std::string::npos ? std::string() : line.substr(0, space);
}

std::string noContentResponse() {
    return "HTTP/1.1 204 No Content\r\n"
           "Content-Length: 0\r\n"
           "Cache-Control: no-store\r\n"
           "Connection: close\r\n\r\n";
}

std::string badRequestResponse() {
    return "HTTP/1.1 400 Bad Request\r\n"
           "Content-Length: 0\r\n"
           "Connection: close\r\n\r\n";
}
} // namespace

void ConnectionRequestServer::serve() {
    while (running_) {
        fd_set fds;
        FD_ZERO(&fds); FD_SET(listenFd_, &fds);
        timeval timeout{1, 0};
        if (select(listenFd_ + 1, &fds, nullptr, nullptr, &timeout) <= 0) continue;
        const int client = accept(listenFd_, nullptr, nullptr);
        if (client < 0) continue;
        char buffer[4096] = {};
        const ssize_t count = recv(client, buffer, sizeof(buffer) - 1, 0);
        const std::string request = count > 0 ? std::string(buffer, static_cast<size_t>(count)) : std::string();
        const std::string line = requestLine(request);
        const std::string method = requestMethod(request);
        Logger::info("Connection Request HTTP: " + (line.empty() ? "<empty>" : line));
        std::string reply;
        if (method.empty()) {
            reply = badRequestResponse();
            Logger::warn("[CONNECTION_REQUEST] bad HTTP request");
        } else if (authorized(request, method)) {
            reply = noContentResponse();
            onRequest_();
        } else {
            reply = challenge();
        }
        send(client, reply.data(), reply.size(), 0);
        close(client);
    }
}

} // namespace tr069
