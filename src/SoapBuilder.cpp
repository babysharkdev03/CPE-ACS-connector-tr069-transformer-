#include "tr069/SoapBuilder.hpp"
#include "tr069/DataModel.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace tr069 {

namespace {
std::string isoTimeNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t value = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&value, &tm);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}
}

std::string SoapBuilder::escape(const std::string& value) {
    std::string out;
    for (char c : value) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '\"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c;
        }
    }
    return out;
}

std::string SoapBuilder::envelope(const std::string& id, const std::string& body) {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
           "<soap-env:Envelope xmlns:soap-env=\"http://schemas.xmlsoap.org/soap/envelope/\" "
           "xmlns:soap-enc=\"http://schemas.xmlsoap.org/soap/encoding/\" "
           "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
           "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
           "xmlns:cwmp=\"urn:dslforum-org:cwmp-1-2\">"
           "<soap-env:Header><cwmp:ID soap-env:mustUnderstand=\"1\">" + escape(id) +
           "</cwmp:ID></soap-env:Header><soap-env:Body>" + body +
           "</soap-env:Body></soap-env:Envelope>";
}

std::string SoapBuilder::inform(const std::string& id, const DataModel& dm,
                                const std::vector<std::string>& events, unsigned retryCount) {
    std::ostringstream body;
    body << "<cwmp:Inform><DeviceId>"
         << "<Manufacturer>" << escape(dm.manufacturer()) << "</Manufacturer>"
         << "<OUI>" << escape(dm.oui()) << "</OUI>"
         << "<ProductClass>" << escape(dm.productClass()) << "</ProductClass>"
         << "<SerialNumber>" << escape(dm.serialNumber()) << "</SerialNumber>"
         << "</DeviceId><Event soap-enc:arrayType=\"cwmp:EventStruct[" << events.size() << "]\">";
    for (const auto& event : events)
        body << "<EventStruct><EventCode>" << escape(event)
             << "</EventCode><CommandKey></CommandKey></EventStruct>";
    const auto params = dm.informParameters();
    body << "</Event><MaxEnvelopes>1</MaxEnvelopes><CurrentTime>" << isoTimeNow()
         << "</CurrentTime><RetryCount>" << retryCount
         << "</RetryCount><ParameterList soap-enc:arrayType=\"cwmp:ParameterValueStruct["
         << params.size() << "]\">";
    for (const auto& parameter : params)
        body << "<ParameterValueStruct><Name>" << escape(parameter.name)
             << "</Name><Value xsi:type=\"" << escape(parameter.type) << "\">"
             << escape(parameter.value) << "</Value></ParameterValueStruct>";
    body << "</ParameterList></cwmp:Inform>";
    return envelope(id, body.str());
}

std::string SoapBuilder::transferComplete(const std::string& id, const TransferResult& r) {
    std::ostringstream body;
    body << "<cwmp:TransferComplete><CommandKey>" << escape(r.commandKey)
         << "</CommandKey><FaultStruct><FaultCode>" << r.faultCode
         << "</FaultCode><FaultString>" << escape(r.faultString)
         << "</FaultString></FaultStruct><StartTime>" << escape(r.startTime)
         << "</StartTime><CompleteTime>" << escape(r.completeTime)
         << "</CompleteTime></cwmp:TransferComplete>";
    return envelope(id, body.str());
}

std::string SoapBuilder::getParameterValuesResponse(const std::string& id,
                                                     const std::vector<ParameterValue>& values) {
    std::ostringstream body;
    body << "<cwmp:GetParameterValuesResponse><ParameterList soap-enc:arrayType=\"cwmp:ParameterValueStruct["
         << values.size() << "]\">";
    for (const auto& value : values)
        body << "<ParameterValueStruct><Name>" << escape(value.name)
             << "</Name><Value xsi:type=\"" << escape(value.type) << "\">"
             << escape(value.value) << "</Value></ParameterValueStruct>";
    body << "</ParameterList></cwmp:GetParameterValuesResponse>";
    return envelope(id, body.str());
}

std::string SoapBuilder::setParameterValuesResponse(const std::string& id, int status) {
    return envelope(id, "<cwmp:SetParameterValuesResponse><Status>" + std::to_string(status) +
                        "</Status></cwmp:SetParameterValuesResponse>");
}

std::string SoapBuilder::getParameterNamesResponse(
    const std::string& id, const std::vector<ParameterInfo>& values) {
    std::ostringstream body;
    body << "<cwmp:GetParameterNamesResponse><ParameterList soap-enc:arrayType=\"cwmp:ParameterInfoStruct["
         << values.size() << "]\">";
    for (const auto& value : values) {
        body << "<ParameterInfoStruct><Name>" << escape(value.name)
             << "</Name><Writable>" << (value.writable ? "1" : "0")
             << "</Writable></ParameterInfoStruct>";
    }
    body << "</ParameterList></cwmp:GetParameterNamesResponse>";
    return envelope(id, body.str());
}
std::string SoapBuilder::rebootResponse(const std::string& id) {
    return envelope(id, "<cwmp:RebootResponse/>");
}
std::string SoapBuilder::downloadResponse(const std::string& id, int status,
                                          const std::string& start, const std::string& complete) {
    return envelope(id, "<cwmp:DownloadResponse><Status>" + std::to_string(status) +
        "</Status><StartTime>" + escape(start) + "</StartTime><CompleteTime>" +
        escape(complete) + "</CompleteTime></cwmp:DownloadResponse>");
}

std::string SoapBuilder::fault(const std::string& id, int code, const std::string& message,
                               const std::vector<std::string>& parameterFaults) {
    std::ostringstream detail;
    detail << "<soap-env:Fault><faultcode>Client</faultcode><faultstring>CWMP fault</faultstring>"
           << "<detail><cwmp:Fault><FaultCode>" << code << "</FaultCode><FaultString>"
           << escape(message) << "</FaultString>";
    for (const auto& name : parameterFaults)
        detail << "<SetParameterValuesFault><ParameterName>" << escape(name)
               << "</ParameterName><FaultCode>9007</FaultCode><FaultString>Invalid parameter value"
               << "</FaultString></SetParameterValuesFault>";
    detail << "</cwmp:Fault></detail></soap-env:Fault>";
    return envelope(id, detail.str());
}

} // namespace tr069
