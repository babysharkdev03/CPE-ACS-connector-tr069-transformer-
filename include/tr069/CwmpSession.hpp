#pragma once

#include "tr069/CwmpStateMachine.hpp"
#include "tr069/DataModel.hpp"
#include "tr069/DownloadManager.hpp"
#include "tr069/HttpClient.hpp"
#include "tr069/RebootHandler.hpp"
#include "cwmp/rpc_handler.hpp"
#include "tr069/SoapParser.hpp"

#include <optional>
#include <string>
#include <vector>

namespace tr069 {

struct SessionOutcome {
    bool success{false};
    bool rebootRequested{false};
    std::string rebootCommandKey;
    std::optional<DownloadRequest> download;
};

class CwmpSession {
public:
    CwmpSession(HttpConfig config, DataModel& dataModel);
    SessionOutcome run(const std::vector<std::string>& events);
    bool runTransferComplete(const TransferResult& result);

private:
    bool beginInform(const std::vector<std::string>& events, const std::string& id);
    SessionOutcome processAcsMessages(std::string firstBody);
    std::string nextId();

    HttpConfig config_;
    DataModel& dataModel_;
    HttpClient http_;
    SoapParser parser_;
    RpcHandler rpcHandler_;
    CwmpStateMachine stateMachine_;
    unsigned sequence_{1};
};

} // namespace tr069
