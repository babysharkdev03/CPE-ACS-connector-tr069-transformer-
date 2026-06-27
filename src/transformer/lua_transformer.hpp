#pragma once

#include "transformer/transformer_api.hpp"

#include <mutex>
#include <string>

struct lua_State;

namespace transformer {

class LuaTransformer final : public TransformerApi {
public:
    LuaTransformer(std::string scriptPath, std::string mapPath);
    ~LuaTransformer() override;

    LuaTransformer(const LuaTransformer&) = delete;
    LuaTransformer& operator=(const LuaTransformer&) = delete;

    bool ready() const { return state_ != nullptr && engineRef_ >= 0; }
    const std::string& lastError() const { return lastError_; }

    std::vector<tr069::ParameterValue> getParameterValues(
        const std::vector<std::string>& paths,
        std::vector<std::string>& invalidPaths,
        bool revealSecrets = false) override;

    SetResult setParameterValues(
        const std::vector<tr069::ParameterValue>& values,
        const std::string& parameterKey) override;

    std::vector<tr069::ParameterInfo> getParameterNames(
        const std::string& path,
        std::string& error) override;

private:
    bool pushFunction(const char* name);
    std::string popLuaError();

    lua_State* state_{nullptr};
    int engineRef_{-1};
    std::string lastError_;
    mutable std::mutex mutex_;
};

} // namespace transformer
