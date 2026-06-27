#include "tr069/CwmpStateMachine.hpp"

namespace tr069 {

bool CwmpStateMachine::transition(CwmpState next) {
    const bool valid =
        (state_ == CwmpState::Idle && next == CwmpState::Connecting) ||
        (state_ == CwmpState::Connecting && next == CwmpState::InformSent) ||
        (state_ == CwmpState::InformSent && (next == CwmpState::RpcProcessing || next == CwmpState::SessionClose)) ||
        (state_ == CwmpState::RpcProcessing && (next == CwmpState::RpcProcessing || next == CwmpState::SessionClose)) ||
        (state_ == CwmpState::SessionClose && next == CwmpState::Idle);
    if (valid) state_ = next;
    return valid;
}

std::string CwmpStateMachine::toString(CwmpState state) {
    switch (state) {
        case CwmpState::Idle: return "IDLE";
        case CwmpState::Connecting: return "CONNECTING";
        case CwmpState::InformSent: return "INFORM_SENT";
        case CwmpState::RpcProcessing: return "RPC_PROCESSING";
        case CwmpState::SessionClose: return "SESSION_CLOSE";
    }
    return "UNKNOWN";
}

} // namespace tr069
