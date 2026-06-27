#pragma once

#include "tr069/ConnectionRequestServer.hpp"
#include "tr069/DataModel.hpp"
#include "tr069/DownloadManager.hpp"
#include "tr069/Logger.hpp"
#include "tr069/RebootHandler.hpp"
#include "tr069/UciAdapter.hpp"
#include "transformer/lua_transformer.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

namespace tr069 {

struct DaemonConfig {
    bool enableCwmp{true};
    std::string acsUrl;
    std::string acsUsername;
    std::string acsPassword;
    std::string caFile;
    bool insecureTls{false};
    bool periodicEnable{true};
    unsigned periodicInterval{300};
    std::string periodicInformTime;
    bool upgradesManaged{false};
    unsigned defaultActiveNotificationThrottle{0};
    bool mockMode{true};
    bool bootstrapDone{false};
    bool pendingRebootEvent{false};
    bool connectionRequestEnable{true};
    unsigned retryMinimumWait{5};
    unsigned retryMultiplier{2000};
    ConnectionRequestConfig connectionRequest;
};

struct DaemonStatus {
    std::string state;
    bool enableCwmp{false};
    std::string acsUrl;
    bool periodicEnable{false};
    unsigned periodicInterval{0};
    bool connectionRequestEnable{false};
    unsigned connectionRequestPort{0};
    std::string connectionRequestUrl;
};

class Tr069Daemon {
public:
    Tr069Daemon();
    int run();
    void requestStop();
    void requestReload();
    void requestReconnect();
    void requestInformNow();
    DaemonStatus status() const;

private:
    enum class WakeReason { Stop, Periodic, ConnectionRequest, Reload, Reconnect, InformNow };
    bool readConfig(DaemonConfig& target);
    bool refreshConfig(bool initial = false);
    void restartConnectionRequestServer();
    void triggerConnectionRequest();
    WakeReason waitForNextInform();
    void setState(const std::string& state);
    static LogLevel parseLogLevel(const std::string& value);

    transformer::LuaTransformer transformer_;
    DataModel dataModel_;
    UciAdapter uci_;
    DaemonConfig config_;
    DownloadManager downloadManager_;
    std::unique_ptr<ConnectionRequestServer> connectionServer_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> connectionRequested_{false};
    std::atomic<bool> reloadRequested_{false};
    std::atomic<bool> reconnectRequested_{false};
    std::atomic<bool> informNowRequested_{false};
    std::mutex waitMutex_;
    std::condition_variable waitCv_;
    mutable std::mutex configMutex_;
    mutable std::mutex stateMutex_;
    std::string state_{"starting"};
};

} // namespace tr069
