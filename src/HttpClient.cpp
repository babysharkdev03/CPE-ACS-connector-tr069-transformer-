#include "tr069/HttpClient.hpp"
#include "tr069/Logger.hpp"

#include <cstdio>
#include <mutex>
#include <utility>

namespace tr069 {

namespace {
std::once_flag curlInitFlag;
void ensureCurlGlobalInit() { curl_global_init(CURL_GLOBAL_DEFAULT); }
}

HttpClient::HttpClient(HttpConfig config) : config_(std::move(config)) {
    std::call_once(curlInitFlag, ensureCurlGlobalInit);
    handle_ = curl_easy_init();
    if (handle_) curl_easy_setopt(handle_, CURLOPT_COOKIEFILE, "");
}

HttpClient::~HttpClient() { if (handle_) curl_easy_cleanup(handle_); }

size_t HttpClient::writeString(char* ptr, size_t size, size_t nmemb, void* userdata) {
    const size_t bytes = size * nmemb;
    static_cast<std::string*>(userdata)->append(ptr, bytes);
    return bytes;
}

size_t HttpClient::writeFile(char* ptr, size_t size, size_t nmemb, void* userdata) {
    return std::fwrite(ptr, size, nmemb, static_cast<FILE*>(userdata));
}

void HttpClient::applyTls(CURL* handle) const {
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, config_.insecureTls ? 0L : 1L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, config_.insecureTls ? 0L : 2L);
    if (!config_.caFile.empty()) curl_easy_setopt(handle, CURLOPT_CAINFO, config_.caFile.c_str());
}

HttpResponse HttpClient::post(const std::string& body) {
    HttpResponse response;
    if (!handle_) { response.error = "curl_easy_init failed"; return response; }
    curl_easy_reset(handle_);
    curl_easy_setopt(handle_, CURLOPT_COOKIEFILE, "");
    curl_easy_setopt(handle_, CURLOPT_URL, config_.url.c_str());
    curl_easy_setopt(handle_, CURLOPT_POST, 1L);
    curl_easy_setopt(handle_, CURLOPT_POSTFIELDS, body.data());
    curl_easy_setopt(handle_, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, &HttpClient::writeString);
    curl_easy_setopt(handle_, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(handle_, CURLOPT_TIMEOUT, config_.timeoutSeconds);
    curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(handle_, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle_, CURLOPT_USERAGENT, "tr069d/0.1");
    if (!config_.username.empty()) {
        curl_easy_setopt(handle_, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
        curl_easy_setopt(handle_, CURLOPT_USERNAME, config_.username.c_str());
        curl_easy_setopt(handle_, CURLOPT_PASSWORD, config_.password.c_str());
    }
    applyTls(handle_);
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: text/xml; charset=utf-8");
    headers = curl_slist_append(headers, "SOAPAction: \"\"");
    headers = curl_slist_append(headers, "Expect:");
    curl_easy_setopt(handle_, CURLOPT_HTTPHEADER, headers);
    char errorBuffer[CURL_ERROR_SIZE] = {};
    curl_easy_setopt(handle_, CURLOPT_ERRORBUFFER, errorBuffer);
    const CURLcode code = curl_easy_perform(handle_);
    curl_slist_free_all(headers);
    if (code != CURLE_OK) response.error = errorBuffer[0] ? errorBuffer : curl_easy_strerror(code);
    curl_easy_getinfo(handle_, CURLINFO_RESPONSE_CODE, &response.status);
    return response;
}

HttpResponse HttpClient::download(const std::string& url, const std::string& username,
                                  const std::string& password, const std::string& outputPath,
                                  long timeoutSeconds) {
    HttpResponse response;
    FILE* file = std::fopen(outputPath.c_str(), "wb");
    if (!file) { response.error = "Cannot open output file"; return response; }
    CURL* downloadHandle = curl_easy_init();
    if (!downloadHandle) { std::fclose(file); response.error = "curl_easy_init failed"; return response; }
    curl_easy_setopt(downloadHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(downloadHandle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(downloadHandle, CURLOPT_WRITEFUNCTION, &HttpClient::writeFile);
    curl_easy_setopt(downloadHandle, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(downloadHandle, CURLOPT_TIMEOUT, timeoutSeconds);
    curl_easy_setopt(downloadHandle, CURLOPT_NOSIGNAL, 1L);
    if (!username.empty()) {
        curl_easy_setopt(downloadHandle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
        curl_easy_setopt(downloadHandle, CURLOPT_USERNAME, username.c_str());
        curl_easy_setopt(downloadHandle, CURLOPT_PASSWORD, password.c_str());
    }
    applyTls(downloadHandle);
    const CURLcode code = curl_easy_perform(downloadHandle);
    if (code != CURLE_OK) response.error = curl_easy_strerror(code);
    curl_easy_getinfo(downloadHandle, CURLINFO_RESPONSE_CODE, &response.status);
    curl_easy_cleanup(downloadHandle);
    std::fclose(file);
    if (!response.ok()) std::remove(outputPath.c_str());
    return response;
}

} // namespace tr069
