#pragma once

#include "tr069/HttpClient.hpp"
#include "tr069/Types.hpp"

#include <string>

namespace tr069 {

class DownloadManager {
public:
    explicit DownloadManager(std::string directory = "/tmp/tr069-downloads");
    TransferResult execute(const DownloadRequest& request, const HttpConfig& baseHttpConfig) const;

private:
    static std::string safeFileName(const std::string& name);
    std::string directory_;
};

} // namespace tr069
