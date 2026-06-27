#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tr069 {

struct ParameterValue {
    std::string name;
    std::string value;
    std::string type{"xsd:string"};
};

struct ParameterInfo {
    std::string name;
    bool writable{false};
};

struct DownloadRequest {
    std::string commandKey;
    std::string fileType;
    std::string url;
    std::string username;
    std::string password;
    std::string targetFileName;
    unsigned delaySeconds{0};
};

struct TransferResult {
    std::string commandKey;
    int faultCode{0};
    std::string faultString;
    std::string startTime;
    std::string completeTime;
};

struct ParsedRpc {
    std::string method;
    std::string cwmpId;
    std::vector<std::string> parameterNames;
    std::vector<ParameterValue> parameterValues;
    std::string parameterPath;
    bool nextLevel{false};
    std::string parameterKey;
    std::string commandKey;
    std::optional<DownloadRequest> download;
};

struct RpcResult {
    std::string responseXml;
    bool requestReboot{false};
    std::string rebootCommandKey;
    std::optional<DownloadRequest> download;
};

} // namespace tr069
