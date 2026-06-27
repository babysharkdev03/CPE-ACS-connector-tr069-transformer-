#include "tr069/DataModel.hpp"

namespace tr069 {

DataModel::DataModel(transformer::TransformerApi& transformer) : transformer_(transformer) {}

std::optional<ParameterValue> DataModel::get(const std::string& path) const {
    std::vector<std::string> invalid;
    const auto values = transformer_.getParameterValues({path}, invalid, false);
    return values.empty() ? std::nullopt : std::optional<ParameterValue>(values.front());
}

std::optional<ParameterValue> DataModel::getInternal(const std::string& path) const {
    std::vector<std::string> invalid;
    const auto values = transformer_.getParameterValues({path}, invalid, true);
    return values.empty() ? std::nullopt : std::optional<ParameterValue>(values.front());
}

std::vector<ParameterValue> DataModel::getMany(
    const std::vector<std::string>& paths, std::vector<std::string>& invalidPaths) const {
    return transformer_.getParameterValues(paths, invalidPaths, false);
}

bool DataModel::set(const ParameterValue& value, std::string& error) {
    const auto result = transformer_.setParameterValues({value}, "");
    error = result.error;
    return result.success;
}

std::vector<ParameterValue> DataModel::informParameters() const {
    const std::vector<std::string> names = {
        "Device.DeviceInfo.Manufacturer",
        "Device.DeviceInfo.ManufacturerOUI",
        "Device.DeviceInfo.ProductClass",
        "Device.DeviceInfo.ModelName",
        "Device.DeviceInfo.SerialNumber",
        "Device.DeviceInfo.SoftwareVersion",
        "Device.RootDataModelVersion",
        "Device.ManagementServer.URL",
        "Device.ManagementServer.PeriodicInformEnable",
        "Device.ManagementServer.PeriodicInformInterval",
        "Device.ManagementServer.ConnectionRequestURL"
    };
    std::vector<std::string> invalid;
    return transformer_.getParameterValues(names, invalid, false);
}

namespace {
std::string valueOr(const DataModel& model, const std::string& path,
                    const std::string& fallback) {
    const auto value = model.get(path);
    return value ? value->value : fallback;
}
}

std::string DataModel::manufacturer() const {
    return valueOr(*this, "Device.DeviceInfo.Manufacturer", "dev");
}
std::string DataModel::oui() const {
    return valueOr(*this, "Device.DeviceInfo.ManufacturerOUI", "000000");
}
std::string DataModel::productClass() const {
    return valueOr(*this, "Device.DeviceInfo.ProductClass", "OpenWrt-RPi5");
}
std::string DataModel::serialNumber() const {
    return valueOr(*this, "Device.DeviceInfo.SerialNumber", "UNKNOWN");
}

} // namespace tr069
