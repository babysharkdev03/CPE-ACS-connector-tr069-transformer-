#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace tr069 {

struct ConnectionRequestConfig {
    std::string bindAddress{"0.0.0.0"};
    unsigned port{7547};
    std::string username;
    std::string password;
    std::string auth{"Digest"};
    std::string realm{"tr069d"};
};

class ConnectionRequestServer {
public:
    ConnectionRequestServer(ConnectionRequestConfig config, std::function<void()> onRequest);
    ~ConnectionRequestServer();
    bool start();
    void stop();

private:
    void serve();
    bool authorized(const std::string& request, const std::string& method) const;
    std::string challenge() const;
    static std::string headerValue(const std::string& request, const std::string& name);
    static std::string digestValue(const std::string& authorization, const std::string& key);

    ConnectionRequestConfig config_;
    std::function<void()> onRequest_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    int listenFd_{-1};
    std::string nonce_;
};

} // namespace tr069
