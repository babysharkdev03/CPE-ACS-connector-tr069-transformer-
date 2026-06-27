#pragma once

#include <curl/curl.h>

#include <string>

namespace tr069 {

struct HttpResponse {
    long status{0};
    std::string body;
    std::string error;
    bool ok() const { return error.empty() && status >= 200 && status < 300; }
};

struct HttpConfig {
    std::string url;
    std::string username;
    std::string password;
    std::string caFile;
    bool insecureTls{false};
    long timeoutSeconds{30};
};

class HttpClient {
public:
    explicit HttpClient(HttpConfig config);
    ~HttpClient();
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpResponse post(const std::string& body);
    HttpResponse download(const std::string& url, const std::string& username,
                          const std::string& password, const std::string& outputPath,
                          long timeoutSeconds = 300);

private:
    static size_t writeString(char* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t writeFile(char* ptr, size_t size, size_t nmemb, void* userdata);
    void applyTls(CURL* handle) const;
    HttpConfig config_;
    CURL* handle_{nullptr};
};

} // namespace tr069
