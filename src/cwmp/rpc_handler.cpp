#include "cwmp/rpc_handler.hpp"

#include "tr069/Logger.hpp"

namespace tr069 {

RpcResult RpcHandler::handle(const ParsedRpc& rpc) const {
    RpcResult result;
    const std::string id = rpc.cwmpId.empty() ? "0" : rpc.cwmpId;
    Logger::info("ACS RPC: " + rpc.method);

    if (rpc.method == "GetParameterValues") {
        for (const auto& name : rpc.parameterNames) {
            Logger::info("ACS get request: " + name);
        }
        std::vector<std::string> invalid;
        const auto values = transformer_.getParameterValues(rpc.parameterNames, invalid, false);
        for (const auto& value : values) {
            const bool secret = value.name == "Device.ManagementServer.Password" ||
                                value.name == "Device.ManagementServer.ConnectionRequestPassword";
            Logger::info("ACS get result: " + value.name + "=" +
                         (secret ? "<redacted>" : value.value));
        }
        for (const auto& name : invalid) {
            Logger::warn("GetParameterValues invalid parameter: " + name);
        }
        result.responseXml = invalid.empty()
            ? SoapBuilder::getParameterValuesResponse(id, values)
            : SoapBuilder::fault(id, 9005, "Invalid parameter name", invalid);
    } else if (rpc.method == "GetParameterNames") {
        Logger::info("ACS get names request: " +
                     (rpc.parameterPath.empty() ? "<empty>" : rpc.parameterPath));
        std::string error;
        const auto names = transformer_.getParameterNames(rpc.parameterPath, error);
        Logger::info("ACS get names result: count=" + std::to_string(names.size()) +
                     (error.empty() ? "" : ", error=" + error));
        result.responseXml = error.empty()
            ? SoapBuilder::getParameterNamesResponse(id, names)
            : SoapBuilder::fault(id, 9005, error);
    } else if (rpc.method == "SetParameterValues") {
        for (const auto& value : rpc.parameterValues) {
            const bool secret = value.name == "Device.ManagementServer.Password" ||
                                value.name == "Device.ManagementServer.ConnectionRequestPassword";
            Logger::info("ACS set request: " + value.name + "=" +
                         (secret ? "<redacted>" : value.value));
        }
        const auto setResult = transformer_.setParameterValues(
            rpc.parameterValues, rpc.parameterKey);
        if (setResult.success) {
            Logger::info("SetParameterValues committed " +
                         std::to_string(rpc.parameterValues.size()) + " parameter(s)");
            std::vector<std::string> names;
            for (const auto& value : rpc.parameterValues) names.push_back(value.name);
            std::vector<std::string> invalid;
            const auto applied = transformer_.getParameterValues(names, invalid, true);
            for (const auto& value : applied) {
                const bool secret = value.name == "Device.ManagementServer.Password" ||
                                    value.name == "Device.ManagementServer.ConnectionRequestPassword";
                Logger::info("ACS set applied: " + value.name + "=" +
                             (secret ? "<redacted>" : value.value));
            }
            result.responseXml = SoapBuilder::setParameterValuesResponse(id);
        } else {
            std::vector<std::string> failed;
            if (!setResult.failedParameter.empty()) failed.push_back(setResult.failedParameter);
            Logger::warn("SetParameterValues transaction rejected: " + setResult.error);
            result.responseXml = SoapBuilder::fault(id, 9003, setResult.error, failed);
        }
    } else if (rpc.method == "Reboot") {
        Logger::warn("ACS reboot request: CommandKey=" +
                     (rpc.commandKey.empty() ? "<empty>" : rpc.commandKey));
        result.responseXml = SoapBuilder::rebootResponse(id);
        result.requestReboot = true;
        result.rebootCommandKey = rpc.commandKey;
    } else if (rpc.method == "Download" && rpc.download) {
        Logger::info("[RPC][Download] ACS download request command_key=" +
                     rpc.download->commandKey + " file_type=" + rpc.download->fileType +
                     " url=" + rpc.download->url);
        result.responseXml = SoapBuilder::downloadResponse(
            id, 1, "0001-01-01T00:00:00Z", "0001-01-01T00:00:00Z");
        result.download = rpc.download;
    } else {
        Logger::warn("Unsupported ACS RPC: " + rpc.method);
        result.responseXml = SoapBuilder::fault(id, 9000, "Method not supported");
    }
    return result;
}

} // namespace tr069
