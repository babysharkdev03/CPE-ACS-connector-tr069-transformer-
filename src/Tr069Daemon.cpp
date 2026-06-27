#include "tr069/Tr069Daemon.hpp"
#include "tr069/CwmpSession.hpp"
#include "tr069/Logger.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <thread>

namespace tr069 {

namespace {
std::string environmentOr(const char* name, const char* fallback) {
    const char* value = std::getenv(name);
    return value && *value ? value : fallback;
}

bool environmentEnabled(const char* name) {
    const char* value = std::getenv(name);
    if (!value) return false;
    return std::string(value) == "1" || std::string(value) == "true" ||
           std::string(value) == "yes" || std::string(value) == "on";
}

std::string internalValue(const DataModel& model, const std::string& path,
                          const std::string& fallback = "") {
    const auto value = model.getInternal(path);
    return value ? value->value : fallback;
}

bool connectionRequestEqual(const ConnectionRequestConfig& a,
                            const ConnectionRequestConfig& b) {
    return a.bindAddress == b.bindAddress && a.port == b.port &&
           a.username == b.username && a.password == b.password &&
           a.auth == b.auth && a.realm == b.realm;
}

bool runtimeConfigEqual(const DaemonConfig& a, const DaemonConfig& b) {
    return a.enableCwmp == b.enableCwmp && a.acsUrl == b.acsUrl &&
           a.acsUsername == b.acsUsername && a.acsPassword == b.acsPassword &&
           a.caFile == b.caFile && a.insecureTls == b.insecureTls &&
           a.periodicEnable == b.periodicEnable &&
           a.periodicInterval == b.periodicInterval &&
           a.periodicInformTime == b.periodicInformTime &&
           a.upgradesManaged == b.upgradesManaged &&
           a.defaultActiveNotificationThrottle == b.defaultActiveNotificationThrottle &&
           a.mockMode == b.mockMode &&
           a.connectionRequestEnable == b.connectionRequestEnable &&
           a.retryMinimumWait == b.retryMinimumWait &&
           a.retryMultiplier == b.retryMultiplier &&
           connectionRequestEqual(a.connectionRequest, b.connectionRequest);
}

std::string boolText(bool value) { return value ? "true" : "false"; }

void logConfigChange(const std::string& key, const std::string& before,
                     const std::string& after) {
    if (before != after) {
        Logger::info("[RUNTIME] config changed " + key + ": " +
                     (before.empty() ? "<empty>" : before) + " -> " +
                     (after.empty() ? "<empty>" : after));
    }
}

void logConfigDiff(const DaemonConfig& before, const DaemonConfig& after) {
    logConfigChange("EnableCWMP", boolText(before.enableCwmp), boolText(after.enableCwmp));
    logConfigChange("ACS URL", before.acsUrl, after.acsUrl);
    logConfigChange("PeriodicInformEnable", boolText(before.periodicEnable),
                    boolText(after.periodicEnable));
    logConfigChange("PeriodicInformInterval", std::to_string(before.periodicInterval),
                    std::to_string(after.periodicInterval));
    logConfigChange("PeriodicInformTime", before.periodicInformTime,
                    after.periodicInformTime);
    logConfigChange("MockMode", boolText(before.mockMode), boolText(after.mockMode));
    logConfigChange("BootstrapDone", boolText(before.bootstrapDone),
                    boolText(after.bootstrapDone));
    logConfigChange("PendingRebootEvent", boolText(before.pendingRebootEvent),
                    boolText(after.pendingRebootEvent));
    logConfigChange("ConnectionRequestEnable", boolText(before.connectionRequestEnable),
                    boolText(after.connectionRequestEnable));
    logConfigChange("ConnectionRequestBind", before.connectionRequest.bindAddress,
                    after.connectionRequest.bindAddress);
    logConfigChange("ConnectionRequestPort",
                    std::to_string(before.connectionRequest.port),
                    std::to_string(after.connectionRequest.port));
    logConfigChange("ConnectionRequestAuth", before.connectionRequest.auth,
                    after.connectionRequest.auth);
}
} // namespace

Tr069Daemon::Tr069Daemon()
    : transformer_(environmentOr("TR69_TRANSFORMER_SCRIPT", "/usr/lib/tr69d/transformer.lua"),
                   environmentOr("TR69_DEVICE_MAP", "/etc/transformer/maps/Device.map")),
      dataModel_(transformer_), uci_(false), downloadManager_("/tmp/tr69-downloads") {}

LogLevel Tr069Daemon::parseLogLevel(const std::string& value) {
    if (value == "debug") return LogLevel::Debug;
    if (value == "warn") return LogLevel::Warn;
    if (value == "error") return LogLevel::Error;
    return LogLevel::Info;
}

bool Tr069Daemon::readConfig(DaemonConfig& target) {
    if (!transformer_.ready()) {
        Logger::error("Transformer initialization failed: " + transformer_.lastError());
        return false;
    }

    target.enableCwmp = internalValue(
        dataModel_, "Device.ManagementServer.EnableCWMP", "true") == "true";
    target.acsUrl = internalValue(dataModel_, "Device.ManagementServer.URL");
    target.acsUsername = internalValue(dataModel_, "Device.ManagementServer.Username");
    target.acsPassword = internalValue(dataModel_, "Device.ManagementServer.Password");
    target.caFile = uci_.get("tr69.settings.ssl_ca_cert_path").value_or("");
    target.insecureTls = !uci_.getBool("tr69.settings.ssl_cn_verification", true);
    target.periodicEnable = internalValue(
        dataModel_, "Device.ManagementServer.PeriodicInformEnable", "true") == "true";
    try {
        target.periodicInterval = std::max(1U, static_cast<unsigned>(std::stoul(
            internalValue(dataModel_, "Device.ManagementServer.PeriodicInformInterval", "300"))));
    } catch (...) {
        target.periodicInterval = 300;
    }
    target.periodicInformTime = internalValue(
        dataModel_, "Device.ManagementServer.PeriodicInformTime", "1970-01-01T00:00:00Z");
    target.upgradesManaged = internalValue(
        dataModel_, "Device.ManagementServer.UpgradesManaged", "false") == "true";
    try {
        target.defaultActiveNotificationThrottle = static_cast<unsigned>(std::stoul(
            internalValue(dataModel_,
                "Device.ManagementServer.DefaultActiveNotificationThrottle", "0")));
    } catch (...) {
        target.defaultActiveNotificationThrottle = 0;
    }
    const bool allowMockReboot = environmentEnabled("TR69_ALLOW_MOCK_REBOOT");
    target.mockMode = allowMockReboot && uci_.getBool("tr69.settings.mock_mode", false);
    if (!allowMockReboot && uci_.getBool("tr69.settings.mock_mode", false)) {
        Logger::warn("[RUNTIME] Ignoring UCI mock_mode=1 because TR69_ALLOW_MOCK_REBOOT is not enabled");
    }
    target.bootstrapDone = uci_.getBool("tr69.settings.bootstrap_done", false);
    target.pendingRebootEvent = uci_.getBool("tr69.settings.pending_reboot_event", false);
    target.connectionRequestEnable = uci_.getBool("tr69.conn_request.enable", true);
    target.connectionRequest.bindAddress =
        uci_.get("tr69.settings.bind").value_or("0.0.0.0");
    target.connectionRequest.port = uci_.getUnsigned("tr69.settings.port", 7547);
    target.connectionRequest.username = internalValue(
        dataModel_, "Device.ManagementServer.ConnectionRequestUsername", "cpe");
    target.connectionRequest.password = internalValue(
        dataModel_, "Device.ManagementServer.ConnectionRequestPassword", "change-me");
    target.connectionRequest.auth = uci_.get("tr69.conn_request.auth").value_or("Digest");
    target.retryMinimumWait = std::max(1U, uci_.getUnsigned("tr69.mgmt_srv.retry_minimum_wait", 5));
    target.retryMultiplier = std::max(1000U,
        uci_.getUnsigned("tr69.mgmt_srv.retry_multiplier", 2000));

    Logger::setLevel(uci_.getBool("tr69.settings.debug", false) ? LogLevel::Debug :
                     parseLogLevel(uci_.get("tr69.settings.log_level").value_or("info")));
    return true;
}

bool Tr069Daemon::refreshConfig(bool initial) {
    DaemonConfig previous;
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        previous = config_;
    }
    DaemonConfig updated = previous;
    if (!readConfig(updated)) return false;
    if (!initial && updated.acsUrl.empty() && !previous.acsUrl.empty()) {
        Logger::warn("[RUNTIME] Ignoring empty ACS URL read-back; preserving previous URL");
        updated.acsUrl = previous.acsUrl;
    }

    const bool connectionChanged =
        updated.connectionRequestEnable != previous.connectionRequestEnable ||
        !connectionRequestEqual(updated.connectionRequest, previous.connectionRequest);
    const bool changed = initial || !runtimeConfigEqual(updated, previous) ||
                         updated.bootstrapDone != previous.bootstrapDone ||
                         updated.pendingRebootEvent != previous.pendingRebootEvent;
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        config_ = std::move(updated);
    }

    if (!initial && connectionChanged) restartConnectionRequestServer();
    if (changed) {
        if (!initial) logConfigDiff(previous, config_);
        Logger::info("Runtime configuration updated: CWMP=" +
                     std::string(config_.enableCwmp ? "enabled" : "disabled") +
                     ", URL=" + (config_.acsUrl.empty() ? "<empty>" : config_.acsUrl) +
                     ", PeriodicInform=" +
                     std::string(config_.periodicEnable ? "enabled/" : "disabled/") +
                     std::to_string(config_.periodicInterval) + "s");
        Logger::info("[RUNTIME] config applied initial=" + boolText(initial) +
                     " mock_mode=" + boolText(config_.mockMode) +
                     " bootstrap_done=" + boolText(config_.bootstrapDone) +
                     " pending_reboot_event=" +
                     boolText(config_.pendingRebootEvent));
    }
    return changed;
}

void Tr069Daemon::restartConnectionRequestServer() {
    Logger::info("[CONNECTION_REQUEST] restarting server enable=" +
                 boolText(config_.connectionRequestEnable) + " bind=" +
                 config_.connectionRequest.bindAddress + " port=" +
                 std::to_string(config_.connectionRequest.port) + " auth=" +
                 config_.connectionRequest.auth);
    if (connectionServer_) {
        connectionServer_->stop();
        connectionServer_.reset();
    }
    if (!config_.connectionRequestEnable) {
        Logger::info("Connection Request server disabled");
        return;
    }
    connectionServer_ = std::make_unique<ConnectionRequestServer>(
        config_.connectionRequest, [this] { triggerConnectionRequest(); });
    if (!connectionServer_->start()) {
        Logger::warn("Connection Request server failed to start");
        connectionServer_.reset();
    }
}

void Tr069Daemon::triggerConnectionRequest() {
    connectionRequested_ = true;
    waitCv_.notify_all();
    Logger::info("Authenticated Connection Request received");
}

Tr069Daemon::WakeReason Tr069Daemon::waitForNextInform() {
    for (;;) {
        std::unique_lock<std::mutex> lock(waitMutex_);
        const auto wake = [this] {
            return stop_ || connectionRequested_ || reloadRequested_ ||
                   reconnectRequested_ || informNowRequested_;
        };
        bool signaled = false;
        if (config_.enableCwmp && !config_.acsUrl.empty() && config_.periodicEnable) {
            signaled = waitCv_.wait_for(lock, std::chrono::seconds(config_.periodicInterval), wake);
        } else {
            waitCv_.wait(lock, wake);
            signaled = true;
        }
        lock.unlock();

        if (stop_) return WakeReason::Stop;
        if (connectionRequested_.exchange(false)) {
            return WakeReason::ConnectionRequest;
        }
        if (reconnectRequested_.exchange(false)) {
            return WakeReason::Reconnect;
        }
        if (informNowRequested_.exchange(false)) {
            return WakeReason::InformNow;
        }
        if (reloadRequested_.exchange(false)) {
            refreshConfig(false);
            return WakeReason::Reload;
        }
        if (!signaled) return WakeReason::Periodic;
    }
}

void Tr069Daemon::setState(const std::string& state) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    state_ = state;
}

int Tr069Daemon::run() {
    if (!refreshConfig(true)) return 1;
    restartConnectionRequestServer();
    setState("idle");

    std::vector<std::string> events;
    if (!config_.bootstrapDone) events.push_back("0 BOOTSTRAP");
    events.push_back("1 BOOT");
    if (config_.pendingRebootEvent) events.push_back("M Reboot");

    while (!stop_) {
        if (!config_.enableCwmp || config_.acsUrl.empty()) {
            if (config_.acsUrl.empty()) Logger::warn("CWMP idle: ACS URL is empty");
            const WakeReason wake = waitForNextInform();
            if (wake == WakeReason::Stop) break;
            if (wake == WakeReason::ConnectionRequest) events = {"6 CONNECTION REQUEST"};
            else if (wake == WakeReason::Periodic || wake == WakeReason::InformNow)
                events = {"2 PERIODIC"};
            else events = {"4 VALUE CHANGE"};
            continue;
        }

        HttpConfig httpConfig{config_.acsUrl, config_.acsUsername, config_.acsPassword,
                              config_.caFile, config_.insecureTls, 30};
        setState("connecting");
        CwmpSession session(httpConfig, dataModel_);
        const SessionOutcome outcome = session.run(events);
        setState(outcome.success ? "idle" : "error");
        if (outcome.success && !config_.bootstrapDone) {
            config_.bootstrapDone = true;
            const bool setOk = uci_.set("tr69.settings.bootstrap_done", "1");
            const bool commitOk = uci_.commit("tr69");
            if (!setOk || !commitOk) {
                Logger::warn("[RUNTIME] bootstrap_done persist failed set=" +
                             boolText(setOk) + " commit=" + boolText(commitOk));
            }
        }
        if (outcome.success && config_.pendingRebootEvent &&
            std::find(events.begin(), events.end(), "M Reboot") != events.end()) {
            config_.pendingRebootEvent = false;
            const bool setOk = uci_.set("tr69.settings.pending_reboot_event", "0");
            const bool commitOk = uci_.commit("tr69");
            if (!setOk || !commitOk) {
                Logger::warn("[RUNTIME] pending_reboot_event clear failed set=" +
                             boolText(setOk) + " commit=" + boolText(commitOk));
            }
        }
        if (outcome.download) {
            const TransferResult transfer = downloadManager_.execute(*outcome.download, httpConfig);
            CwmpSession transferSession(httpConfig, dataModel_);
            transferSession.runTransferComplete(transfer);
            refreshConfig(false);
        }
        if (outcome.rebootRequested) {
            config_.pendingRebootEvent = true;
            const bool setOk = uci_.set("tr69.settings.pending_reboot_event", "1");
            const bool commitOk = uci_.commit("tr69");
            if (!setOk || !commitOk) {
                Logger::warn("[RUNTIME] pending_reboot_event persist failed set=" +
                             boolText(setOk) + " commit=" + boolText(commitOk));
            }
            const bool rebootOk = RebootHandler(config_.mockMode).execute(
                outcome.rebootCommandKey);
            Logger::warn("[RUNTIME] reboot request handled mock_mode=" +
                         boolText(config_.mockMode) + " scheduled=" +
                         boolText(rebootOk));
            if (config_.mockMode) {
                refreshConfig(false);
                std::this_thread::sleep_for(std::chrono::seconds(2));
                events = {"1 BOOT", "M Reboot"};
                continue;
            }
        }
        const bool reloadDuringSession = reloadRequested_.exchange(false);
        const bool changedAfterSession = refreshConfig(false);
        if (reloadDuringSession || changedAfterSession) {
            events = {"4 VALUE CHANGE"};
            continue;
        }
        if (stop_) break;

        const WakeReason wake = waitForNextInform();
        if (wake == WakeReason::Stop) break;
        if (wake == WakeReason::ConnectionRequest) events = {"6 CONNECTION REQUEST"};
        else if (wake == WakeReason::Periodic || wake == WakeReason::InformNow)
            events = {"2 PERIODIC"};
        else events = {"4 VALUE CHANGE"};
    }
    if (connectionServer_) connectionServer_->stop();
    Logger::info("tr69d stopped");
    return 0;
}

void Tr069Daemon::requestReload() {
    reloadRequested_ = true;
    waitCv_.notify_all();
}

void Tr069Daemon::requestReconnect() {
    reconnectRequested_ = true;
    waitCv_.notify_all();
}

void Tr069Daemon::requestInformNow() {
    informNowRequested_ = true;
    waitCv_.notify_all();
}

DaemonStatus Tr069Daemon::status() const {
    DaemonStatus result;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        result.state = state_;
    }
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        result.enableCwmp = config_.enableCwmp;
        result.acsUrl = config_.acsUrl;
        result.periodicEnable = config_.periodicEnable;
        result.periodicInterval = config_.periodicInterval;
        result.connectionRequestEnable = config_.connectionRequestEnable;
        result.connectionRequestPort = config_.connectionRequest.port;
    }
    const auto connectionRequestUrl = dataModel_.get("Device.ManagementServer.ConnectionRequestURL");
    result.connectionRequestUrl = connectionRequestUrl ? connectionRequestUrl->value : "";
    return result;
}

void Tr069Daemon::requestStop() {
    stop_ = true;
    waitCv_.notify_all();
}

} // namespace tr069
