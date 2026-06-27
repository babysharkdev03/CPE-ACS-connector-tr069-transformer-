#pragma once

#include "tr069/Types.hpp"

#include <string>
#include <vector>

namespace transformer {

struct SetResult {
    bool success{false};
    std::string error;
    std::string failedParameter;
};

class TransformerApi {
public:
    virtual ~TransformerApi() = default;

    virtual std::vector<tr069::ParameterValue> getParameterValues(
        const std::vector<std::string>& paths,
        std::vector<std::string>& invalidPaths,
        bool revealSecrets = false) = 0;

    virtual SetResult setParameterValues(
        const std::vector<tr069::ParameterValue>& values,
        const std::string& parameterKey) = 0;

    virtual std::vector<tr069::ParameterInfo> getParameterNames(
        const std::string& path,
        std::string& error) = 0;
};

} // namespace transformer
