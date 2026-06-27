#pragma once

#include "tr069/Types.hpp"

#include <optional>
#include <string>

namespace tr069 {

class SoapParser {
public:
    std::optional<ParsedRpc> parseRpc(const std::string& xml, std::string& error) const;
    bool isMethod(const std::string& xml, const std::string& localName) const;
};

} // namespace tr069
