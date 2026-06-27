#include "tr069/CwmpStateMachine.hpp"

#include <cassert>

int main() {
    using tr069::CwmpState;
    tr069::CwmpStateMachine machine;
    assert(machine.state() == CwmpState::Idle);
    assert(!machine.transition(CwmpState::RpcProcessing));
    assert(machine.transition(CwmpState::Connecting));
    assert(machine.transition(CwmpState::InformSent));
    assert(machine.transition(CwmpState::RpcProcessing));
    assert(machine.transition(CwmpState::RpcProcessing));
    assert(machine.transition(CwmpState::SessionClose));
    assert(machine.transition(CwmpState::Idle));
    return 0;
}
