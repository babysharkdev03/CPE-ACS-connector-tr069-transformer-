#pragma once

#include "tr069/Types.hpp"
#include "transformer/transformer_api.hpp"

#include <optional>
#include <string>
#include <vector>

namespace tr069 {

class DataModel {
public:
    explicit DataModel(transformer::TransformerApi& transformer);
    std::optional<ParameterValue> get(const std::string& path) const;
    std::optional<ParameterValue> getInternal(const std::string& path) const;
    std::vector<ParameterValue> getMany(const std::vector<std::string>& paths,
                                        std::vector<std::string>& invalidPaths) const;
    bool set(const ParameterValue& value, std::string& error);
    std::vector<ParameterValue> informParameters() const;

    std::string manufacturer() const;
    std::string oui() const;
    std::string productClass() const;
    std::string serialNumber() const;
    transformer::TransformerApi& transformer() const { return transformer_; }

private:
    transformer::TransformerApi& transformer_;
};

} // namespace tr069
