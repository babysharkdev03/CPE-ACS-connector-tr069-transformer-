#pragma once

#include "tr069/Types.hpp"

#include <string>
#include <vector>

namespace tr069 {

class DataModel;

class SoapBuilder {
public:
    static std::string inform(const std::string& id, const DataModel& dataModel,
                              const std::vector<std::string>& eventCodes,
                              unsigned retryCount = 0);
    static std::string transferComplete(const std::string& id, const TransferResult& result);
    static std::string getParameterValuesResponse(const std::string& id,
                                                   const std::vector<ParameterValue>& values);
    static std::string setParameterValuesResponse(const std::string& id, int status = 0);
    static std::string getParameterNamesResponse(const std::string& id,
                                                  const std::vector<ParameterInfo>& values);
    static std::string rebootResponse(const std::string& id);
    static std::string downloadResponse(const std::string& id, int status,
                                        const std::string& startTime,
                                        const std::string& completeTime);
    static std::string fault(const std::string& id, int code, const std::string& message,
                             const std::vector<std::string>& parameterFaults = {});

private:
    static std::string envelope(const std::string& id, const std::string& body);
    static std::string escape(const std::string& value);
};

} // namespace tr069
