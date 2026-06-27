#include "transformer/lua_transformer.hpp"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <utility>

#if LUA_VERSION_NUM == 501
#ifndef LUA_OK
#define LUA_OK 0
#endif
#define lua_rawlen lua_objlen
static int lua_absindex(lua_State* state, int index) {
    return index > 0 || index <= LUA_REGISTRYINDEX ? index : lua_gettop(state) + index + 1;
}
#endif

namespace transformer {

namespace {
std::string fieldString(lua_State* state, int index, const char* field) {
    index = lua_absindex(state, index);
    lua_getfield(state, index, field);
    const char* text = lua_tostring(state, -1);
    std::string value = text ? text : "";
    lua_pop(state, 1);
    return value;
}

bool fieldBool(lua_State* state, int index, const char* field) {
    index = lua_absindex(state, index);
    lua_getfield(state, index, field);
    const bool value = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    return value;
}
}

LuaTransformer::LuaTransformer(std::string scriptPath, std::string mapPath) {
    state_ = luaL_newstate();
    if (!state_) {
        lastError_ = "cannot create Lua state";
        return;
    }
    luaL_openlibs(state_);
    if (luaL_loadfile(state_, scriptPath.c_str()) != LUA_OK ||
        lua_pcall(state_, 0, 1, 0) != LUA_OK) {
        lastError_ = popLuaError();
        lua_close(state_);
        state_ = nullptr;
        return;
    }
    if (!lua_istable(state_, -1)) {
        lastError_ = "transformer.lua did not return a module table";
        lua_close(state_);
        state_ = nullptr;
        return;
    }
    lua_getfield(state_, -1, "new");
    lua_remove(state_, -2);
    if (!lua_isfunction(state_, -1)) {
        lastError_ = "transformer.lua does not export new()";
        lua_close(state_);
        state_ = nullptr;
        return;
    }
    lua_pushlstring(state_, mapPath.data(), mapPath.size());
    if (lua_pcall(state_, 1, 2, 0) != LUA_OK) {
        lastError_ = popLuaError();
        lua_close(state_);
        state_ = nullptr;
        return;
    }
    if (!lua_istable(state_, -2)) {
        lastError_ = lua_isstring(state_, -1) ? lua_tostring(state_, -1)
                                              : "cannot create Transformer engine";
        lua_close(state_);
        state_ = nullptr;
        return;
    }
    lua_pushvalue(state_, -2);
    engineRef_ = luaL_ref(state_, LUA_REGISTRYINDEX);
    lua_settop(state_, 0);
}

LuaTransformer::~LuaTransformer() {
    if (state_) {
        if (engineRef_ >= 0) luaL_unref(state_, LUA_REGISTRYINDEX, engineRef_);
        lua_close(state_);
    }
}

std::string LuaTransformer::popLuaError() {
    const char* error = state_ ? lua_tostring(state_, -1) : nullptr;
    std::string result = error ? error : "unknown Lua error";
    if (state_) lua_pop(state_, 1);
    return result;
}

bool LuaTransformer::pushFunction(const char* name) {
    if (!ready()) return false;
    lua_rawgeti(state_, LUA_REGISTRYINDEX, engineRef_);
    lua_getfield(state_, -1, name);
    lua_remove(state_, -2);
    if (!lua_isfunction(state_, -1)) {
        lua_pop(state_, 1);
        lastError_ = std::string("Transformer method missing: ") + name;
        return false;
    }
    return true;
}

std::vector<tr069::ParameterValue> LuaTransformer::getParameterValues(
    const std::vector<std::string>& paths,
    std::vector<std::string>& invalidPaths,
    bool revealSecrets) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<tr069::ParameterValue> result;
    if (!pushFunction("getParameterValues")) {
        invalidPaths.insert(invalidPaths.end(), paths.begin(), paths.end());
        return result;
    }
    lua_createtable(state_, static_cast<int>(paths.size()), 0);
    for (std::size_t i = 0; i < paths.size(); ++i) {
        lua_pushlstring(state_, paths[i].data(), paths[i].size());
        lua_rawseti(state_, -2, static_cast<lua_Integer>(i + 1));
    }
    lua_pushboolean(state_, revealSecrets);
    if (lua_pcall(state_, 2, 2, 0) != LUA_OK) {
        lastError_ = popLuaError();
        invalidPaths.insert(invalidPaths.end(), paths.begin(), paths.end());
        lua_settop(state_, 0);
        return result;
    }
    if (lua_istable(state_, -2)) {
        const auto count = lua_rawlen(state_, -2);
        for (std::size_t i = 1; i <= count; ++i) {
            lua_rawgeti(state_, -2, static_cast<lua_Integer>(i));
            if (lua_istable(state_, -1)) {
                result.push_back({fieldString(state_, -1, "name"),
                                  fieldString(state_, -1, "value"),
                                  fieldString(state_, -1, "type")});
            }
            lua_pop(state_, 1);
        }
    }
    if (lua_istable(state_, -1)) {
        const auto count = lua_rawlen(state_, -1);
        for (std::size_t i = 1; i <= count; ++i) {
            lua_rawgeti(state_, -1, static_cast<lua_Integer>(i));
            const char* text = lua_tostring(state_, -1);
            if (text) invalidPaths.emplace_back(text);
            lua_pop(state_, 1);
        }
    }
    lua_settop(state_, 0);
    return result;
}

SetResult LuaTransformer::setParameterValues(
    const std::vector<tr069::ParameterValue>& values,
    const std::string& parameterKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    SetResult result;
    if (!pushFunction("setParameterValues")) {
        result.error = lastError_;
        return result;
    }
    lua_createtable(state_, static_cast<int>(values.size()), 0);
    for (std::size_t i = 0; i < values.size(); ++i) {
        lua_createtable(state_, 0, 3);
        lua_pushlstring(state_, values[i].name.data(), values[i].name.size());
        lua_setfield(state_, -2, "name");
        lua_pushlstring(state_, values[i].value.data(), values[i].value.size());
        lua_setfield(state_, -2, "value");
        lua_pushlstring(state_, values[i].type.data(), values[i].type.size());
        lua_setfield(state_, -2, "type");
        lua_rawseti(state_, -2, static_cast<lua_Integer>(i + 1));
    }
    lua_pushlstring(state_, parameterKey.data(), parameterKey.size());
    if (lua_pcall(state_, 2, 2, 0) != LUA_OK) {
        result.error = popLuaError();
        lua_settop(state_, 0);
        return result;
    }
    result.success = lua_toboolean(state_, -2) != 0;
    if (!result.success) {
        if (lua_istable(state_, -1)) {
            result.error = fieldString(state_, -1, "message");
            result.failedParameter = fieldString(state_, -1, "path");
        } else if (lua_isstring(state_, -1)) {
            result.error = lua_tostring(state_, -1);
        }
        if (result.error.empty()) result.error = "Transformer transaction failed";
    }
    lua_settop(state_, 0);
    return result;
}

std::vector<tr069::ParameterInfo> LuaTransformer::getParameterNames(
    const std::string& path, std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<tr069::ParameterInfo> result;
    if (!pushFunction("getParameterNames")) {
        error = lastError_;
        return result;
    }
    lua_pushlstring(state_, path.data(), path.size());
    if (lua_pcall(state_, 1, 2, 0) != LUA_OK) {
        error = popLuaError();
        lua_settop(state_, 0);
        return result;
    }
    if (!lua_istable(state_, -2)) {
        error = lua_isstring(state_, -1) ? lua_tostring(state_, -1) : "unknown parameter path";
        lua_settop(state_, 0);
        return result;
    }
    const auto count = lua_rawlen(state_, -2);
    for (std::size_t i = 1; i <= count; ++i) {
        lua_rawgeti(state_, -2, static_cast<lua_Integer>(i));
        if (lua_istable(state_, -1)) {
            result.push_back({fieldString(state_, -1, "name"),
                              fieldBool(state_, -1, "writable")});
        }
        lua_pop(state_, 1);
    }
    lua_settop(state_, 0);
    return result;
}

} // namespace transformer
