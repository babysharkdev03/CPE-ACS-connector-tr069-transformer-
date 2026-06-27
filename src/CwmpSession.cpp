#include "tr069/CwmpSession.hpp"
#include "tr069/Logger.hpp"
#include "tr069/SoapBuilder.hpp"

#include <sstream>
#include <utility>

namespace tr069 {

namespace {
std::string joinEvents(const std::vector<std::string>& events) {
    std::ostringstream out;
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (i) out << ",";
        out << events[i];
    }
    return out.str();
}
} // namespace

CwmpSession::CwmpSession(HttpConfig config, DataModel& dataModel)
    : config_(std::move(config)), dataModel_(dataModel), http_(config_),
      rpcHandler_(dataModel_.transformer()) {}

std::string CwmpSession::nextId() { return "tr069d-" + std::to_string(sequence_++); }

bool CwmpSession::beginInform(const std::vector<std::string>& events, const std::string& id) {
    stateMachine_.reset();
    stateMachine_.transition(CwmpState::Connecting);
    Logger::info("CWMP Inform: events=" +
                 (events.empty() ? "<none>" : joinEvents(events)));
    Logger::info("CWMP state: CONNECTING");
    const auto response = http_.post(SoapBuilder::inform(id, dataModel_, events));
    if (!response.ok()) {
        Logger::error("[CWMP][SESSION] Inform failed id=" + id + ": " + (response.error.empty()
            ? "HTTP " + std::to_string(response.status) : response.error));
        stateMachine_.reset();
        return false;
    }
    if (!parser_.isMethod(response.body, "InformResponse")) {
        Logger::error("[CWMP][SESSION] ACS did not return InformResponse id=" + id);
        stateMachine_.reset();
        return false;
    }
    stateMachine_.transition(CwmpState::InformSent);
    Logger::info("CWMP state: INFORM_SENT");
    return true;
}

SessionOutcome CwmpSession::processAcsMessages(std::string body) {
    SessionOutcome outcome;
    while (!body.empty()) {
        std::string error;
        const auto rpc = parser_.parseRpc(body, error);
        if (!rpc) {
            Logger::error("[CWMP][SESSION] Cannot parse ACS RPC: " + error);
            return outcome;
        }
        stateMachine_.transition(CwmpState::RpcProcessing);
        const RpcResult handled = rpcHandler_.handle(*rpc);
        const auto response = http_.post(handled.responseXml);
        if (!response.ok()) {
            Logger::error("[CWMP][SESSION] RPC response POST failed method=" + rpc->method +
                          ": " + response.error);
            return outcome;
        }
        if (handled.requestReboot) {
            outcome.rebootRequested = true;
            outcome.rebootCommandKey = handled.rebootCommandKey;
        }
        if (handled.download) outcome.download = handled.download;
        body = response.body;
        if (outcome.rebootRequested || outcome.download) break;
    }
    stateMachine_.transition(CwmpState::SessionClose);
    Logger::info("CWMP state: SESSION_CLOSE");
    stateMachine_.transition(CwmpState::Idle);
    outcome.success = true;
    return outcome;
}

SessionOutcome CwmpSession::run(const std::vector<std::string>& events) {
    SessionOutcome failed;
    if (!beginInform(events, nextId())) return failed;
    const auto response = http_.post("");
    if (!response.ok()) {
        Logger::error("[CWMP][SESSION] Empty CWMP POST failed: " + response.error);
        return failed;
    }
    return processAcsMessages(response.body);
}

bool CwmpSession::runTransferComplete(const TransferResult& result) {
    if (!beginInform({"7 TRANSFER COMPLETE", "M Download"}, nextId())) return false;
    auto response = http_.post(SoapBuilder::transferComplete(nextId(), result));
    if (!response.ok() || !parser_.isMethod(response.body, "TransferCompleteResponse")) {
        Logger::error("[CWMP][SESSION] TransferComplete was not acknowledged");
        return false;
    }
    response = http_.post("");
    if (!response.ok()) return false;
    return processAcsMessages(response.body).success;
}

} // namespace tr069
