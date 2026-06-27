#pragma once

#include "tr069/Tr069Daemon.hpp"

struct ubus_context;
struct ubus_object;
struct ubus_request_data;
struct blob_attr;

namespace tr069 {

class UbusServer {
public:
    explicit UbusServer(Tr069Daemon& daemon);
    ~UbusServer();

    bool initialize();
    void run();
    void stop();

    // libubus C callbacks; public only so they can be referenced by the method table.
    static int reload(ubus_context*, ubus_object*, ubus_request_data*,
                      const char*, blob_attr*);
    static int reconnect(ubus_context*, ubus_object*, ubus_request_data*,
                         const char*, blob_attr*);
    static int informNow(ubus_context*, ubus_object*, ubus_request_data*,
                         const char*, blob_attr*);
    static int status(ubus_context*, ubus_object*, ubus_request_data*,
                      const char*, blob_attr*);

private:
    static UbusServer* instance_;
    Tr069Daemon& daemon_;
    ubus_context* context_{nullptr};
    ubus_object* object_{nullptr};
};

} // namespace tr069
