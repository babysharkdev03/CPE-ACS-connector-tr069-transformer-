#pragma once

#include <mutex>
#include <string>

namespace tr069 {

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3 };

class Logger {
public:
    static void setLevel(LogLevel level);
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warn(const std::string& message);
    static void error(const std::string& message);

private:
    static void log(LogLevel level, const std::string& message);
    static std::mutex mutex_;
    static LogLevel level_;
};

} // namespace tr069
