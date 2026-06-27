#include "tr069/RebootHandler.hpp"
#include "tr069/Logger.hpp"

#include <cstdlib>
#include <sys/wait.h>

namespace tr069 {

bool RebootHandler::execute(const std::string& commandKey) const {
    if (mockMode_) {
        Logger::warn("[RPC][Reboot] Mock reboot requested; CommandKey=" + commandKey);
        return true;
    }
    Logger::warn("[RPC][Reboot] Scheduling real /sbin/reboot; CommandKey=" + commandKey);
    const int status = std::system("(sleep 1; /sbin/reboot) >/dev/null 2>&1 &");
    const bool ok = status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (!ok) Logger::error("[RPC][Reboot] Failed to schedule /sbin/reboot");
    return ok;
}

} // namespace tr069
