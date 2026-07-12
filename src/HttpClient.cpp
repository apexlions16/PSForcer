#include "HttpClient.h"
#include "FileUtil.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <sys/stat.h>

#if defined(PSFORCER_ORBIS)
#include <orbis/Http.h>
#include <orbis/Net.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h>
#endif

namespace psforcer {

HttpClient::HttpClient()
    : initialized_(false)
#if defined(PSFORCER_ORBIS)
    , netPoolId_(0), sslContextId_(0), httpContextId_(0)
#endif
{}

HttpClient::~HttpClient() {
    shutdown();
}

bool HttpClient::initialize(std::string& error) {
    if (initialized_) return true;
#if defined(PSFORCER_ORBIS)
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET) < 0 ||
        sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP) < 0 ||
        sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL) < 0) {
        error = "Network system modules could not be loaded";
        return false;
    }
    const int netResult = sceNetInit();
    (void)netResult;
    netPoolId_ = sceNetPoolCreate("PSForcerNet", 64 * 1024, 0);
    if (netPoolId_ < 0) {
        error = "sceNetPoolCreate failed";
        return false;
    }
    sslContextId_ = sceSslInit(256 * 1024);
    if (sslContextId_ < 0) {
        error = "sceSslInit failed";
        shutdown();
        return false;
    }
    httpContextId_ = sceHttpInit(netPoolId_, sslContextId_, 512 * 1024);
    if (httpContextId_ < 0) {
        error = "sceHttpInit failed";
        shutdown();
        return false;
    }
#else
    (void)error;
#endif
    initialized_ = true;
    return true;
}

void HttpClient::shutdown() {
#if defined(PSFORCER_ORBIS)
    if (httpContextId_ > 0) sceHttpTerm(httpContextId_);
    if (sslContextId_ > 0) sceSslTerm(sslContextId_);
    if (netPoolId_ > 0) sceNetPoolDestroy(netPoolId_);
    httpContextId_ = 0;
    sslContextId_ = 0;
    netPoolId_ = 0;
#endif
    initialized_ = false;
}

bool HttpClient::download(const std::string& url,
                          const std::string& destination,
                          bool resume,
                          const HttpProgressCallback& progress,
                          std::atomic<bool>* cancel,
                          std::string& error) {
    if (!initialized_ && !initialize(error)) return false;
    const size_t slash = destination.find_last_of('/');
    if (slash != std::string::npos && !ensureDirectory(destination.substr(0, slash))) {
        error = "Download directory could not be created";
        return false;
    }

#if defined(PSFORCER_ORBIS)
    int templateId = 0;
    int connectionId = 0;
    int requestId = 0;
    FILE* output = NULL;
    bool success = false;
    uint64_t existing = resume ? fileSize(destination) : 0;
    int32_t statusCode = 0;
    int contentLengthType = 0;
    size_t responseLength = 0;
    uint64_t total = 0;
    uint64_t downloaded = 0;
    uint8_t buffer[128 * 1024];

    templateId = sceHttpCreateTemplate(httpContextId_, "PSForcer/0.10 (PlayStation 4)", ORBIS_HTTP_VERSION_1_1, 1);
    if (templateId < 0) {
        error = "sceHttpCreateTemplate failed";
        return false;
    }
    connectionId = sceHttpCreateConnectionWithURL(templateId, url.c_str(), true);
    if (connectionId < 0) {
        error = "sceHttpCreateConnectionWithURL failed";
        goto cleanup;
    }
    requestId = sceHttpCreateRequestWithURL(connectionId, ORBIS_METHOD_GET, url.c_str(), 0);
    if (requestId < 0) {
        error = "sceHttpCreateRequestWithURL failed";
        goto cleanup;
    }
    if (existing > 0) {
        std::ostringstream range;
        range << "bytes=" << existing << '-';
        sceHttpAddRequestHeader(requestId, "Range", range.str().c_str(), 0);
    }
    if (sceHttpSendRequest(requestId, NULL, 0) < 0) {
        error = "sceHttpSendRequest failed";
        goto cleanup;
    }

    if (sceHttpGetStatusCode(requestId, &statusCode) < 0) {
        error = "Unable to read HTTP status";
        goto cleanup;
    }
    if (statusCode != 200 && statusCode != 206) {
        std::ostringstream status;
        status << "HTTP request failed with status " << statusCode;
        error = status.str();
        goto cleanup;
    }
    if (statusCode == 200 && existing > 0) existing = 0;

    sceHttpGetResponseContentLength(requestId, &contentLengthType, &responseLength);
    total = existing + static_cast<uint64_t>(responseLength);

    output = std::fopen(destination.c_str(), existing > 0 ? "ab" : "wb");
    if (!output) {
        error = "Unable to open destination file";
        goto cleanup;
    }

    downloaded = existing;
    while (true) {
        if (cancel && cancel->load()) {
            error = "Download cancelled";
            goto cleanup;
        }
        const int read = sceHttpReadData(requestId, buffer, sizeof(buffer));
        if (read < 0) {
            error = "Network read failed";
            goto cleanup;
        }
        if (read == 0) break;
        const size_t written = std::fwrite(buffer, 1, static_cast<size_t>(read), output);
        if (written != static_cast<size_t>(read)) {
            error = "Storage write failed";
            goto cleanup;
        }
        downloaded += written;
        if (progress) progress(HttpProgress{downloaded, total});
    }
    std::fflush(output);
    success = true;

cleanup:
    if (output) std::fclose(output);
    if (requestId > 0) sceHttpDeleteRequest(requestId);
    if (connectionId > 0) sceHttpDeleteConnection(connectionId);
    if (templateId > 0) sceHttpDeleteTemplate(templateId);
    return success;
#else
    if (url.compare(0, 7, "file://") != 0) {
        error = "Desktop build only supports file:// downloads";
        return false;
    }
    const std::string sourcePath = url.substr(7);
    FILE* input = std::fopen(sourcePath.c_str(), "rb");
    if (!input) {
        error = "Unable to open file:// source";
        return false;
    }
    FILE* output = std::fopen(destination.c_str(), "wb");
    if (!output) {
        std::fclose(input);
        error = "Unable to open destination";
        return false;
    }
    const uint64_t total = fileSize(sourcePath);
    uint64_t copied = 0;
    uint8_t buffer[64 * 1024];
    bool success = true;
    while (true) {
        if (cancel && cancel->load()) {
            error = "Download cancelled";
            success = false;
            break;
        }
        const size_t read = std::fread(buffer, 1, sizeof(buffer), input);
        if (read > 0 && std::fwrite(buffer, 1, read, output) != read) {
            error = "Storage write failed";
            success = false;
            break;
        }
        copied += read;
        if (progress) progress(HttpProgress{copied, total});
        if (read < sizeof(buffer)) break;
    }
    std::fclose(output);
    std::fclose(input);
    return success;
#endif
}

}  // namespace psforcer
