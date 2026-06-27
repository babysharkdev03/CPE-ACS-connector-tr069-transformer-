#pragma once

#include <string>

namespace tr069 {

enum class CwmpState { Idle, Connecting, InformSent, RpcProcessing, SessionClose };

class CwmpStateMachine {
public:
    CwmpState state() const { return state_; }
    bool transition(CwmpState next);
    void reset() { state_ = CwmpState::Idle; }
    static std::string toString(CwmpState state);

private:
    CwmpState state_{CwmpState::Idle};
};

} // namespace tr069
