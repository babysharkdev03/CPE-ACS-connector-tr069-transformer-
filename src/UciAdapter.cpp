#include "tr069/UciAdapter.hpp"
#include "tr069/Logger.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

namespace tr069 {

UciAdapter::UciAdapter(bool mockMode) : mockMode_(mockMode) {}

bool UciAdapter::validKey(const std::string& key) {
    if (key.empty()) return false;
    for (unsigned char c : key) {
        if (!(std::isalnum(c) || c == '_' || c == '-' || c == '.' || c == '@' ||
              c == '[' || c == ']')) return false;
    }
    return true;
}

std::string UciAdapter::shellQuote(const std::string& value) {
    std::string out("'");
    for (char c : value) out += (c == '\'') ? "'\\''" : std::string(1, c);
    return out + "'";
}

std::optional<std::string> UciAdapter::runCapture(const std::string& command) {
    std::array<char, 256> buffer{};
    std::string output;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return std::nullopt;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) output += buffer.data();
    const int status = pclose(pipe);
    if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) return std::nullopt;
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) output.pop_back();
    return output;
}

std::string uciBinary() {
    return access("/sbin/uci", X_OK) == 0 ? "/sbin/uci" : "uci";
}

std::optional<std::string> UciAdapter::get(const std::string& key) const {
    if (!validKey(key)) {
        Logger::error("Rejected invalid UCI key: " + key);
        return std::nullopt;
    }
    const auto value = runCapture(uciBinary() + " -q get " + key + " 2>/dev/null");
    return value;
}

bool UciAdapter::set(const std::string& key, const std::string& value) const {
    if (!validKey(key)) {
        Logger::error("Rejected invalid UCI set key: " + key);
        return false;
    }
    if (mockMode_) {
        return true;
    }
    const std::string command = uciBinary() + " -q set " + key + "=" + shellQuote(value);
    const int status = std::system(command.c_str());
    const bool ok = status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (!ok) Logger::error("UCI set failed: " + key);
    return ok;
}

bool UciAdapter::commit(const std::string& package) const {
    if (!validKey(package)) {
        Logger::error("Rejected invalid UCI commit package: " + package);
        return false;
    }
    if (mockMode_) {
        return true;
    }
    const int status = std::system((uciBinary() + " -q commit " + package).c_str());
    const bool ok = status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (!ok) Logger::error("UCI commit failed: " + package);
    return ok;
}

bool UciAdapter::getBool(const std::string& key, bool fallback) const {
    const auto value = get(key);
    if (!value) return fallback;
    return *value == "1" || *value == "true" || *value == "yes" || *value == "on";
}

unsigned UciAdapter::getUnsigned(const std::string& key, unsigned fallback) const {
    const auto value = get(key);
    if (!value) return fallback;
    try {
        size_t consumed = 0;
        const unsigned long parsed = std::stoul(*value, &consumed);
        return consumed == value->size() ? static_cast<unsigned>(parsed) : fallback;
    } catch (...) {
        return fallback;
    }
}

} // namespace tr069
