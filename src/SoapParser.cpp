#include "tr069/SoapParser.hpp"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <memory>

namespace tr069 {

namespace {
struct DocDeleter { void operator()(xmlDoc* doc) const { xmlFreeDoc(doc); } };
using DocPtr = std::unique_ptr<xmlDoc, DocDeleter>;

std::string nodeText(xmlNode* node) {
    if (!node) return {};
    xmlChar* text = xmlNodeGetContent(node);
    if (!text) return {};
    std::string value(reinterpret_cast<char*>(text));
    xmlFree(text);
    return value;
}

xmlNode* child(xmlNode* parent, const char* localName) {
    if (!parent) return nullptr;
    for (xmlNode* n = parent->children; n; n = n->next)
        if (n->type == XML_ELEMENT_NODE && xmlStrEqual(n->name, BAD_CAST localName)) return n;
    return nullptr;
}

xmlNode* findDescendant(xmlNode* parent, const char* localName) {
    if (!parent) return nullptr;
    for (xmlNode* n = parent; n; n = n->next) {
        if (n->type == XML_ELEMENT_NODE && xmlStrEqual(n->name, BAD_CAST localName)) return n;
        if (xmlNode* found = findDescendant(n->children, localName)) return found;
    }
    return nullptr;
}

std::string localType(xmlNode* node) {
    if (!node) return "xsd:string";
    xmlChar* value = xmlGetNsProp(node, BAD_CAST "type", BAD_CAST "http://www.w3.org/2001/XMLSchema-instance");
    if (!value) return "xsd:string";
    std::string result(reinterpret_cast<char*>(value));
    xmlFree(value);
    return result;
}
}

std::optional<ParsedRpc> SoapParser::parseRpc(const std::string& xml, std::string& error) const {
    if (xml.empty()) { error = "Empty SOAP document"; return std::nullopt; }
    DocPtr doc(xmlReadMemory(xml.data(), static_cast<int>(xml.size()), "cwmp.xml", nullptr,
                             XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING));
    if (!doc) { error = "Malformed XML"; return std::nullopt; }
    xmlNode* root = xmlDocGetRootElement(doc.get());
    xmlNode* body = findDescendant(root, "Body");
    if (!body) { error = "SOAP Body missing"; return std::nullopt; }
    xmlNode* method = nullptr;
    for (xmlNode* n = body->children; n; n = n->next)
        if (n->type == XML_ELEMENT_NODE) { method = n; break; }
    if (!method) { error = "SOAP method missing"; return std::nullopt; }

    ParsedRpc rpc;
    rpc.method = reinterpret_cast<const char*>(method->name);
    xmlNode* id = findDescendant(findDescendant(root, "Header"), "ID");
    rpc.cwmpId = nodeText(id);

    if (rpc.method == "GetParameterValues") {
        xmlNode* names = child(method, "ParameterNames");
        for (xmlNode* n = names ? names->children : nullptr; n; n = n->next)
            if (n->type == XML_ELEMENT_NODE && xmlStrEqual(n->name, BAD_CAST "string"))
                rpc.parameterNames.push_back(nodeText(n));
    } else if (rpc.method == "GetParameterNames") {
        rpc.parameterPath = nodeText(child(method, "ParameterPath"));
        const std::string nextLevel = nodeText(child(method, "NextLevel"));
        rpc.nextLevel = nextLevel == "1" || nextLevel == "true";
    } else if (rpc.method == "SetParameterValues") {
        xmlNode* list = child(method, "ParameterList");
        for (xmlNode* item = list ? list->children : nullptr; item; item = item->next) {
            if (item->type != XML_ELEMENT_NODE || !xmlStrEqual(item->name, BAD_CAST "ParameterValueStruct")) continue;
            xmlNode* value = child(item, "Value");
            rpc.parameterValues.push_back({nodeText(child(item, "Name")), nodeText(value), localType(value)});
        }
        rpc.parameterKey = nodeText(child(method, "ParameterKey"));
    } else if (rpc.method == "Reboot") {
        rpc.commandKey = nodeText(child(method, "CommandKey"));
    } else if (rpc.method == "Download") {
        DownloadRequest request;
        request.commandKey = nodeText(child(method, "CommandKey"));
        request.fileType = nodeText(child(method, "FileType"));
        request.url = nodeText(child(method, "URL"));
        request.username = nodeText(child(method, "Username"));
        request.password = nodeText(child(method, "Password"));
        request.targetFileName = nodeText(child(method, "TargetFileName"));
        const std::string delay = nodeText(child(method, "DelaySeconds"));
        try { request.delaySeconds = delay.empty() ? 0U : static_cast<unsigned>(std::stoul(delay)); }
        catch (...) { request.delaySeconds = 0; }
        rpc.download = request;
    }
    return rpc;
}

bool SoapParser::isMethod(const std::string& xml, const std::string& localName) const {
    std::string error;
    const auto parsed = parseRpc(xml, error);
    return parsed && parsed->method == localName;
}

} // namespace tr069
