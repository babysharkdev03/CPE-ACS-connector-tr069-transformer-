#pragma once

#include <string>

namespace tr069 {

class RebootHandler {
public:
    explicit RebootHandler(bool mockMode) : mockMode_(mockMode) {}
    bool execute(const std::string& commandKey) const;

private:
    bool mockMode_;
};

} // namespace tr069
