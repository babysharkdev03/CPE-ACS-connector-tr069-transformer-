#include "tr069/Logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace tr069 {

std::mutex Logger::mutex_;
LogLevel Logger::level_ = LogLevel::Info;

void Logger::setLevel(LogLevel level) { level_ = level; }
void Logger::debug(const std::string& message) { log(LogLevel::Debug, message); }
void Logger::info(const std::string& message) { log(LogLevel::Info, message); }
void Logger::warn(const std::string& message) { log(LogLevel::Warn, message); }
void Logger::error(const std::string& message) { log(LogLevel::Error, message); }

void Logger::log(LogLevel level, const std::string& message) {
    if (static_cast<int>(level) < static_cast<int>(level_)) return;
    const char* names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    const auto now = std::chrono::system_clock::now();
    const std::time_t when = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&when, &tm);
    std::lock_guard<std::mutex> lock(mutex_);
    std::cerr << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S%z") << " ["
              << names[static_cast<int>(level)] << "] " << message << '\n';
}

} // namespace tr069
