#include "tr069/Md5.hpp"
#include "tr069/SoapParser.hpp"

#include <cassert>
#include <string>

int main() {
    assert(tr069::md5Hex("") == "d41d8cd98f00b204e9800998ecf8427e");
    assert(tr069::md5Hex("abc") == "900150983cd24fb0d6963f7d28e17f72");

    const std::string xml =
        "<soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "xmlns:cwmp=\"urn:dslforum-org:cwmp-1-2\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
        "<soap:Header><cwmp:ID>42</cwmp:ID></soap:Header><soap:Body>"
        "<cwmp:SetParameterValues><ParameterList><ParameterValueStruct>"
        "<Name>Device.ManagementServer.PeriodicInformInterval</Name>"
        "<Value xsi:type=\"xsd:unsignedInt\">120</Value>"
        "</ParameterValueStruct></ParameterList><ParameterKey>demo</ParameterKey>"
        "</cwmp:SetParameterValues></soap:Body></soap:Envelope>";
    tr069::SoapParser parser;
    std::string error;
    auto rpc = parser.parseRpc(xml, error);
    assert(rpc);
    assert(rpc->cwmpId == "42");
    assert(rpc->method == "SetParameterValues");
    assert(rpc->parameterValues.size() == 1);
    assert(rpc->parameterValues[0].value == "120");
    return 0;
}
