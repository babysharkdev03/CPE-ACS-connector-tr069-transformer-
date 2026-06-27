#include "tr069/ConnectionRequestServer.hpp"
#include "tr069/Md5.hpp"

#include <arpa/inet.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
std::string exchange(const std::string& request) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(17547);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    assert(connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    send(fd, request.data(), request.size(), 0);
    char buffer[4096] = {};
    const ssize_t count = recv(fd, buffer, sizeof(buffer) - 1, 0);
    close(fd);
    return count > 0 ? std::string(buffer, static_cast<size_t>(count)) : std::string();
}

std::string quoted(const std::string& text, const std::string& key) {
    const size_t start = text.find(key + "=\"");
    assert(start != std::string::npos);
    const size_t valueStart = start + key.size() + 2;
    const size_t end = text.find('"', valueStart);
    return text.substr(valueStart, end - valueStart);
}
}

int main() {
    std::atomic<bool> called{false};
    tr069::ConnectionRequestConfig config{"127.0.0.1", 17547, "cpe", "secret", "Digest", "tr069d"};
    tr069::ConnectionRequestServer server(config, [&called] { called = true; });
    assert(server.start());

    const std::string first = exchange("GET /connection-request HTTP/1.1\r\nHost: localhost\r\n\r\n");
    assert(first.find("401 Unauthorized") != std::string::npos);
    const std::string nonce = quoted(first, "nonce");
    const std::string nc = "00000001", cnonce = "abcdef", qop = "auth";
    const std::string ha1 = tr069::md5Hex("cpe:tr069d:secret");
    const std::string ha2 = tr069::md5Hex("GET:/connection-request");
    const std::string response = tr069::md5Hex(ha1 + ":" + nonce + ":" + nc + ":" +
                                               cnonce + ":" + qop + ":" + ha2);
    const std::string auth =
        "Authorization: Digest username=\"cpe\", realm=\"tr069d\", nonce=\"" + nonce +
        "\", uri=\"/connection-request\", response=\"" + response + "\", qop=auth, nc=" + nc +
        ", cnonce=\"" + cnonce + "\"\r\n";
    const std::string second = exchange("GET /connection-request HTTP/1.1\r\nHost: localhost\r\n" + auth + "\r\n");
    assert(second.find("204 No Content") != std::string::npos);
    for (int i = 0; i < 20 && !called; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    assert(called);
    server.stop();

    called = false;
    config.auth = "None";
    tr069::ConnectionRequestServer noneServer(config, [&called] { called = true; });
    assert(noneServer.start());
    const std::string third = exchange("POST /connection-request HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n");
    assert(third.find("204 No Content") != std::string::npos);
    for (int i = 0; i < 20 && !called; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    assert(called);
    noneServer.stop();
    return 0;
}
