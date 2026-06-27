#include "tr069/DownloadManager.hpp"
#include "tr069/Logger.hpp"

#include <chrono>
#include <cctype>
#include <cerrno>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <thread>
#include <utility>

namespace tr069 {

namespace {
std::string nowIso() {
    const std::time_t value = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&value, &tm);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}
}

DownloadManager::DownloadManager(std::string directory) : directory_(std::move(directory)) {}

std::string DownloadManager::safeFileName(const std::string& name) {
    std::string result;
    for (unsigned char c : name)
        if (std::isalnum(c) || c == '.' || c == '_' || c == '-') result += static_cast<char>(c);
    return result.empty() ? "download.bin" : result;
}

TransferResult DownloadManager::execute(const DownloadRequest& request,
                                        const HttpConfig& baseHttpConfig) const {
    TransferResult result;
    result.commandKey = request.commandKey;
    if (request.delaySeconds) std::this_thread::sleep_for(std::chrono::seconds(request.delaySeconds));
    result.startTime = nowIso();
    if (mkdir(directory_.c_str(), 0755) != 0 && errno != EEXIST) {
        result.faultCode = 9010;
        result.faultString = "Cannot create download directory";
        result.completeTime = nowIso();
        return result;
    }
    std::string name = request.targetFileName;
    if (name.empty()) {
        const auto slash = request.url.find_last_of('/');
        name = slash == std::string::npos ? "download.bin" : request.url.substr(slash + 1);
    }
    const std::string path = directory_ + "/" + safeFileName(name);
    HttpClient client(baseHttpConfig);
    const auto response = client.download(request.url, request.username, request.password, path);
    if (!response.ok()) {
        result.faultCode = 9010;
        result.faultString = response.error.empty() ? "HTTP status " + std::to_string(response.status)
                                                   : response.error;
        Logger::error("Download failed: " + result.faultString);
    } else {
        Logger::info("Download completed: " + path);
    }
    result.completeTime = nowIso();
    return result;
}

} // namespace tr069
