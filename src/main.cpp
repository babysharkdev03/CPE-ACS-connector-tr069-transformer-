#include "tr069/Logger.hpp"
#include "tr069/Tr069Daemon.hpp"
#ifdef TR69_WITH_UBUS
#include "tr069/UbusServer.hpp"
#endif

#include <pthread.h>
#include <signal.h>
#include <thread>

int main() {
#ifdef TR69_WITH_UBUS
    tr069::Tr069Daemon daemon;
    tr069::UbusServer ubus(daemon);
    if (!ubus.initialize()) return 1;

    int result = 0;
    std::thread daemonThread([&] {
        result = daemon.run();
        ubus.stop();
    });
    tr069::Logger::info("Starting tr69d with ubus/uloop runtime");
    ubus.run();
    daemon.requestStop();
    daemonThread.join();
    return result;
#else
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    sigaddset(&signals, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &signals, nullptr);

    tr069::Tr069Daemon daemon;
    std::thread signalThread([&daemon, signals]() mutable {
        for (;;) {
            int signal = 0;
            sigwait(&signals, &signal);
            if (signal == SIGHUP) {
                daemon.requestReload();
                continue;
            }
            daemon.requestStop();
            break;
        }
    });
    tr069::Logger::info("Starting tr69d");
    const int result = daemon.run();
    pthread_kill(signalThread.native_handle(), SIGTERM);
    signalThread.join();
    return result;
#endif
}
