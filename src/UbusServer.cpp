#include "tr069/UbusServer.hpp"

#include "tr069/Logger.hpp"

extern "C" {
#include <libubox/blobmsg.h>
#include <libubox/uloop.h>
#include <libubus.h>
}

#include <unistd.h>

namespace tr069 {

UbusServer* UbusServer::instance_ = nullptr;

namespace {
const ubus_method methods[] = {
    {"reload", UbusServer::reload, 0, 0, nullptr, 0},
    {"reconnect", UbusServer::reconnect, 0, 0, nullptr, 0},
    {"inform_now", UbusServer::informNow, 0, 0, nullptr, 0},
    {"status", UbusServer::status, 0, 0, nullptr, 0},
};

ubus_object_type objectType{};
ubus_object objectDefinition{};

void sendAccepted(ubus_context* context, ubus_request_data* request,
                  const char* action) {
    blob_buf buffer{};
    blob_buf_init(&buffer, 0);
    blobmsg_add_u8(&buffer, "accepted", 1);
    blobmsg_add_string(&buffer, "action", action);
    ubus_send_reply(context, request, buffer.head);
    blob_buf_free(&buffer);
}
} // namespace

UbusServer::UbusServer(Tr069Daemon& daemon) : daemon_(daemon) {}

UbusServer::~UbusServer() {
    if (context_ && object_) ubus_remove_object(context_, object_);
    if (context_) ubus_free(context_);
    uloop_done();
    if (instance_ == this) instance_ = nullptr;
}

bool UbusServer::initialize() {
    if (uloop_init() != 0) {
        Logger::error("uloop initialization failed");
        return false;
    }
    context_ = ubus_connect(nullptr);
    if (!context_) {
        Logger::error("Cannot connect to ubus");
        uloop_done();
        return false;
    }
    ubus_add_uloop(context_);
    objectType.name = "tr69";
    objectType.methods = methods;
    objectType.n_methods = static_cast<int>(sizeof(methods) / sizeof(methods[0]));
    objectDefinition.name = "tr69";
    objectDefinition.type = &objectType;
    objectDefinition.methods = methods;
    objectDefinition.n_methods = objectType.n_methods;
    object_ = &objectDefinition;
    instance_ = this;
    const int result = ubus_add_object(context_, object_);
    if (result != 0) {
        Logger::error(std::string("Cannot register ubus object tr69: ") +
                      ubus_strerror(result));
        instance_ = nullptr;
        return false;
    }
    Logger::info("Registered ubus object: tr69");
    return true;
}

void UbusServer::run() { uloop_run(); }

void UbusServer::stop() { uloop_end(); }

int UbusServer::reload(ubus_context* context, ubus_object*,
                       ubus_request_data* request, const char*, blob_attr*) {
    if (!instance_) return UBUS_STATUS_UNKNOWN_ERROR;
    instance_->daemon_.requestReload();
    sendAccepted(context, request, "reload");
    return UBUS_STATUS_OK;
}

int UbusServer::reconnect(ubus_context* context, ubus_object*,
                          ubus_request_data* request, const char*, blob_attr*) {
    if (!instance_) return UBUS_STATUS_UNKNOWN_ERROR;
    instance_->daemon_.requestReconnect();
    sendAccepted(context, request, "reconnect");
    return UBUS_STATUS_OK;
}

int UbusServer::informNow(ubus_context* context, ubus_object*,
                          ubus_request_data* request, const char*, blob_attr*) {
    if (!instance_) return UBUS_STATUS_UNKNOWN_ERROR;
    instance_->daemon_.requestInformNow();
    sendAccepted(context, request, "inform_now");
    return UBUS_STATUS_OK;
}

int UbusServer::status(ubus_context* context, ubus_object*,
                       ubus_request_data* request, const char*, blob_attr*) {
    if (!instance_) return UBUS_STATUS_UNKNOWN_ERROR;
    const DaemonStatus status = instance_->daemon_.status();
    blob_buf buffer{};
    blob_buf_init(&buffer, 0);
    blobmsg_add_u8(&buffer, "running", 1);
    blobmsg_add_u32(&buffer, "pid", static_cast<uint32_t>(getpid()));
    blobmsg_add_string(&buffer, "state", status.state.c_str());
    blobmsg_add_u8(&buffer, "enable_cwmp", status.enableCwmp ? 1 : 0);
    blobmsg_add_string(&buffer, "acs_url", status.acsUrl.c_str());
    blobmsg_add_u8(&buffer, "periodic_inform_enable", status.periodicEnable ? 1 : 0);
    blobmsg_add_u32(&buffer, "periodic_inform_interval", status.periodicInterval);
    blobmsg_add_u8(&buffer, "connection_request_enable",
                   status.connectionRequestEnable ? 1 : 0);
    blobmsg_add_u32(&buffer, "connection_request_port",
                    status.connectionRequestPort);
    blobmsg_add_string(&buffer, "connection_request_url",
                       status.connectionRequestUrl.c_str());
    ubus_send_reply(context, request, buffer.head);
    blob_buf_free(&buffer);
    return UBUS_STATUS_OK;
}

} // namespace tr069
