#pragma once

#include <optional>
#include <string>

namespace tr069 {

class UciAdapter {
public:
    explicit UciAdapter(bool mockMode = false);
    std::optional<std::string> get(const std::string& key) const;
    bool set(const std::string& key, const std::string& value) const;
    bool commit(const std::string& package) const;
    bool getBool(const std::string& key, bool fallback) const;
    unsigned getUnsigned(const std::string& key, unsigned fallback) const;

private:
    static bool validKey(const std::string& key);
    static std::string shellQuote(const std::string& value);
    static std::optional<std::string> runCapture(const std::string& command);
    bool mockMode_;
};

} // namespace tr069
