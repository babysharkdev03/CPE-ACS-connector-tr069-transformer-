#include "transformer/lua_transformer.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {
void writeExecutable(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    file << content;
    file.close();
    chmod(path.c_str(), 0755);
}
} // namespace

int main() {
    const std::string database = "/tmp/tr69d-transformer-test-" +
                                 std::to_string(static_cast<long>(getpid())) + ".db";
    const std::string binDir = "/tmp/tr69d-transformer-bin-" +
                               std::to_string(static_cast<long>(getpid()));
    std::remove(database.c_str());
    mkdir(binDir.c_str(), 0700);
    writeExecutable(binDir + "/ubus",
        "#!/bin/sh\n"
        "if [ \"$1\" = call ] && [ \"$2\" = network.interface.wwan ]; then\n"
        "cat <<'JSON'\n"
        "{ \"up\": true, \"l3_device\": \"phy0-sta0\", "
        "\"ipv4-address\": [ { \"address\": \"192.168.88.123\", \"mask\": 24 } ] }\n"
        "JSON\n"
        "exit 0\n"
        "fi\n"
        "exit 1\n");
    writeExecutable(binDir + "/ip",
        "#!/bin/sh\n"
        "if [ \"$1\" = -4 ] && [ \"$2\" = -o ]; then\n"
        "  echo '7: phy0-sta0 inet 192.168.88.123/24 brd 192.168.88.255 scope global phy0-sta0'\n"
        "  exit 0\n"
        "fi\n"
        "if [ \"$1\" = -4 ] && [ \"$2\" = route ]; then\n"
        "  echo '192.168.88.72 dev phy0-sta0 src 192.168.88.123 uid 0'\n"
        "  exit 0\n"
        "fi\n"
        "exit 1\n");
    const char* oldPath = std::getenv("PATH");
    const std::string path = binDir + ":" + (oldPath ? oldPath : "");
    setenv("PATH", path.c_str(), 1);
    setenv("TRANSFORMER_MOCK_DB", database.c_str(), 1);
    {
        std::ofstream file(database);
        file << "tr69.settings.interface=wwan\n";
        file << "tr69.settings.port=7547\n";
        file << "tr69.mgmt_srv.url=http://192.168.88.72:7547/\n";
        file << "tr69.conn_request.url=http://acs.test:7547/connection-request\n";
    }

    transformer::LuaTransformer api(
        std::string(TEST_SOURCE_DIR) + "/transformer/transformer.lua",
        std::string(TEST_SOURCE_DIR) + "/transformer/maps/Device.map");
    assert(api.ready());

    const std::vector<tr069::ParameterValue> writable = {
        {"Device.ManagementServer.EnableCWMP", "true", "xsd:boolean"},
        {"Device.ManagementServer.URL", "http://acs.test:7547/", "xsd:string"},
        {"Device.ManagementServer.Username", "acs-user", "xsd:string"},
        {"Device.ManagementServer.Password", "secret", "xsd:string"},
        {"Device.ManagementServer.PeriodicInformEnable", "true", "xsd:boolean"},
        {"Device.ManagementServer.PeriodicInformInterval", "60", "xsd:unsignedInt"},
        {"Device.ManagementServer.PeriodicInformTime", "2026-06-21T00:00:00Z", "xsd:dateTime"},
        {"Device.ManagementServer.ConnectionRequestUsername", "cpe-user", "xsd:string"},
        {"Device.ManagementServer.ConnectionRequestPassword", "cpe-secret", "xsd:string"},
        {"Device.ManagementServer.ConnectionRequestAuthentication", "None", "xsd:string"},
        {"Device.ManagementServer.UpgradesManaged", "false", "xsd:boolean"},
        {"Device.ManagementServer.DefaultActiveNotificationThrottle", "10", "xsd:unsignedInt"},
        {"Device.ManagementServer.CWMPRetryMinimumWaitInterval", "15", "xsd:unsignedInt"},
        {"Device.ManagementServer.CWMPRetryIntervalMultiplier", "2000", "xsd:unsignedInt"}
    };
    const auto success = api.setParameterValues(writable, "key-good");
    assert(success.success);

    std::vector<std::string> allNames;
    for (const auto& value : writable) allNames.push_back(value.name);
    std::vector<std::string> allInvalid;
    const auto allValues = api.getParameterValues(allNames, allInvalid, true);
    assert(allInvalid.empty() && allValues.size() == writable.size());
    for (std::size_t i = 0; i < writable.size(); ++i) {
        assert(allValues[i].name == writable[i].name);
        assert(allValues[i].value == writable[i].value);
    }

    std::vector<std::string> invalid;
    auto values = api.getParameterValues({
        "Device.ManagementServer.URL",
        "Device.ManagementServer.Password",
        "Device.ManagementServer.ParameterKey"
    }, invalid, false);
    assert(invalid.empty() && values.size() == 3);
    assert(values[0].value == "http://acs.test:7547/");
    assert(values[1].value.empty());
    assert(values[2].value == "key-good");

    invalid.clear();
    values = api.getParameterValues({"Device.ManagementServer.Password"}, invalid, true);
    assert(values.size() == 1 && values[0].value == "secret");

    const auto failure = api.setParameterValues({
        {"Device.ManagementServer.PeriodicInformInterval", "not-a-number", "xsd:unsignedInt"}
    }, "key-bad");
    assert(!failure.success);

    const auto typeFailure = api.setParameterValues({
        {"Device.ManagementServer.PeriodicInformInterval", "600", "xsd:string"}
    }, "key-type-bad");
    assert(!typeFailure.success);

    const auto emptyUrlFailure = api.setParameterValues({
        {"Device.ManagementServer.URL", "", "xsd:string"}
    }, "key-empty-url");
    assert(!emptyUrlFailure.success);

    invalid.clear();
    values = api.getParameterValues(
        {"Device.ManagementServer.PeriodicInformInterval"}, invalid, false);
    assert(values.size() == 1 && values[0].value == "60");

    invalid.clear();
    values = api.getParameterValues({"Device.ManagementServer.ParameterKey"}, invalid, false);
    assert(values.size() == 1 && values[0].value == "key-good");

    invalid.clear();
    values = api.getParameterValues(
        {"Device.ManagementServer.ConnectionRequestURL"}, invalid, false);
    assert(invalid.empty() && values.size() == 1);
    assert(values[0].value == "http://192.168.88.123:7547/connection-request");

    std::string error;
    const auto names = api.getParameterNames("Device.ManagementServer.", error);
    assert(error.empty() && names.size() >= 15);

    std::remove(database.c_str());
    std::remove((binDir + "/ubus").c_str());
    std::remove((binDir + "/ip").c_str());
    rmdir(binDir.c_str());

    unsetenv("TRANSFORMER_MOCK_DB");
    const std::string uciDatabase = "/tmp/tr69d-transformer-uci-test-" +
                                    std::to_string(static_cast<long>(getpid())) + ".db";
    const std::string uciBinDir = "/tmp/tr69d-transformer-uci-bin-" +
                                  std::to_string(static_cast<long>(getpid()));
    std::remove(uciDatabase.c_str());
    std::remove((uciDatabase + ".staged").c_str());
    mkdir(uciBinDir.c_str(), 0700);
    writeExecutable(uciBinDir + "/uci",
        "#!/bin/sh\n"
        "exec " + std::string(TEST_SOURCE_DIR) + "/tests/fake_uci.sh \"$@\"\n");
    setenv("PATH", (uciBinDir + ":" + (oldPath ? oldPath : "")).c_str(), 1);
    setenv("UCI_TEST_DB", uciDatabase.c_str(), 1);
    {
        std::ofstream file(uciDatabase);
        file << "tr69.mgmt_srv.enable=true\n";
        file << "tr69.mgmt_srv.username=\n";
        file << "tr69.mgmt_srv.password=\n";
        file << "tr69.mgmt_srv.parameter_key=\n";
        file << "tr69.periodic_inform.enable=true\n";
        file << "tr69.periodic_inform.interval=300\n";
    }
    assert(std::system("uci -q set tr69.mgmt_srv.url=http://192.168.88.72:7547/") == 0);

    transformer::LuaTransformer uciApi(
        std::string(TEST_SOURCE_DIR) + "/transformer/transformer.lua",
        std::string(TEST_SOURCE_DIR) + "/transformer/maps/Device.map");
    assert(uciApi.ready());
    const auto stagedSuccess = uciApi.setParameterValues({
        {"Device.ManagementServer.PeriodicInformInterval", "109", "xsd:unsignedInt"}
    }, "staged-url");
    assert(stagedSuccess.success);

    invalid.clear();
    values = uciApi.getParameterValues({
        "Device.ManagementServer.URL",
        "Device.ManagementServer.PeriodicInformInterval",
        "Device.ManagementServer.ParameterKey"
    }, invalid, true);
    assert(invalid.empty() && values.size() == 3);
    assert(values[0].value == "http://192.168.88.72:7547/");
    assert(values[1].value == "109");
    assert(values[2].value == "staged-url");

    std::remove(uciDatabase.c_str());
    std::remove((uciDatabase + ".staged").c_str());
    std::remove((uciBinDir + "/uci").c_str());
    rmdir(uciBinDir.c_str());
    return 0;
}
