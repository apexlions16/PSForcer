#include "HttpClient.h"
#include "FileUtil.h"
#include "HttpMessages.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <sys/stat.h>
#include <vector>
#include <mutex>

#if defined(PSFORCER_ORBIS)
#include <orbis/Http.h>
#include <orbis/Net.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h>
#endif

namespace psforcer {

#if defined(PSFORCER_ORBIS)
namespace {
std::mutex gHttpRuntimeMutex;
int gNetPoolId = 0;
int gSslContextId = 0;
int gHttpContextId = 0;
unsigned int gHttpUsers = 0;

bool isHuggingFaceUrl(const std::string& url) {
    return url.compare(0, 23, "https://huggingface.co/") == 0;
}

std::string huggingFaceToken() {
    return trim(readFirstLine("/data/psforcer/hf_token.txt"));
}
}
#endif

HttpClient::HttpClient() : initialized_(false)
#if defined(PSFORCER_ORBIS)
, netPoolId_(0), sslContextId_(0), httpContextId_(0)
#endif
{}

HttpClient::~HttpClient() { shutdown(); }

bool HttpClient::initialize(std::string& error) {
    if (initialized_) return true;
#if defined(PSFORCER_ORBIS)
    std::lock_guard<std::mutex> runtimeLock(gHttpRuntimeMutex);

    if (gHttpContextId <= 0) {
        if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET) < 0 ||
            sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP) < 0 ||
            sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL) < 0) {
            error = PSF_AG_MODULLERI;
            return false;
        }

        const int netInitResult = sceNetInit();
        (void)netInitResult;

        gNetPoolId = sceNetPoolCreate("PSForcerAg", 512 * 1024, 0);
        if (gNetPoolId < 0) {
            error = PSF_AG_HAVUZU;
            gNetPoolId = 0;
            return false;
        }

        gSslContextId = sceSslInit(1024 * 1024);
        if (gSslContextId < 0) {
            error = PSF_SSL;
            sceNetPoolDestroy(gNetPoolId);
            gNetPoolId = 0;
            gSslContextId = 0;
            return false;
        }

        gHttpContextId = sceHttpInit(gNetPoolId, gSslContextId, 2 * 1024 * 1024);
        if (gHttpContextId < 0) {
            error = PSF_HTTP;
            sceSslTerm();
            sceNetPoolDestroy(gNetPoolId);
            gNetPoolId = 0;
            gSslContextId = 0;
            gHttpContextId = 0;
            return false;
        }
    }

    ++gHttpUsers;
    netPoolId_ = gNetPoolId;
    sslContextId_ = gSslContextId;
    httpContextId_ = gHttpContextId;
#else
    (void)error;
#endif
    initialized_ = true;
    return true;
}

void HttpClient::shutdown() {
    if (!initialized_) return;
#if defined(PSFORCER_ORBIS)
    std::lock_guard<std::mutex> runtimeLock(gHttpRuntimeMutex);
    if (gHttpUsers > 0) --gHttpUsers;
    if (gHttpUsers == 0 && gHttpContextId > 0) {
        sceHttpTerm(gHttpContextId);
        sceSslTerm();
        sceNetPoolDestroy(gNetPoolId);
        gHttpContextId = 0;
        gSslContextId = 0;
        gNetPoolId = 0;
    }
    httpContextId_ = sslContextId_ = netPoolId_ = 0;
#endif
    initialized_ = false;
}

bool HttpClient::download(const std::string& url, const std::string& destination, bool resume,
                          const HttpProgressCallback& progress, std::atomic<bool>* cancel,
                          std::string& error) {
    if (!initialized_ && !initialize(error)) return false;
    const size_t slash = destination.find_last_of('/');
    if (slash != std::string::npos && !ensureDirectory(destination.substr(0, slash))) {
        error = PSF_KLASOR;
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

    std::vector<uint8_t> buffer(64 * 1024);

    templateId = sceHttpCreateTemplate(httpContextId_, "PSForcer/0.16", ORBIS_HTTP_VERSION_1_1, 1);
    if (templateId < 0) {
        error = PSF_SABLON;
        return false;
    }

    connectionId = sceHttpCreateConnectionWithURL(templateId, url.c_str(), true);
    if (connectionId < 0) {
        error = PSF_BAGLANTI;
        goto cleanup;
    }

    requestId = sceHttpCreateRequestWithURL(connectionId, ORBIS_METHOD_GET, url.c_str(), 0);
    if (requestId < 0) {
        error = PSF_ISTEK;
        goto cleanup;
    }

    sceHttpAddRequestHeader(requestId, "Accept", "application/octet-stream", 0);
    if (isHuggingFaceUrl(url)) {
        const std::string token = huggingFaceToken();
        if (!token.empty()) {
            const std::string authorization = "Bearer " + token;
            sceHttpAddRequestHeader(requestId, "Authorization", authorization.c_str(), 0);
        }
    }

    sceHttpSetResolveTimeOut(requestId, 15 * 1000 * 1000);
    sceHttpSetConnectTimeOut(requestId, 20 * 1000 * 1000);
    sceHttpSetSendTimeOut(requestId, 20 * 1000 * 1000);

    if (existing > 0) {
        std::ostringstream range;
        range << "bytes=" << existing << '-';
        if (sceHttpAddRequestHeader(requestId, "Range", range.str().c_str(), 0) < 0) {
            existing = 0;
        }
    }

    if (sceHttpSendRequest(requestId, NULL, 0) < 0) {
        error = PSF_GONDER;
        goto cleanup;
    }

    if (sceHttpGetStatusCode(requestId, &statusCode) < 0) {
        error = PSF_DURUM;
        goto cleanup;
    }

    if (statusCode != 200 && statusCode != 206) {
        if (statusCode == 401 && isHuggingFaceUrl(url)) {
            error = "Hugging Face erişimi reddetti (401): hf_token.txt gerekli veya depo herkese açık olmalı";
        } else if (statusCode == 403 && isHuggingFaceUrl(url)) {
            error = "Hugging Face erişimi reddetti (403): belirteç iznini ve depo erişimini denetleyin";
        } else {
            std::ostringstream durum;
            durum << "İndirme isteği başarısız oldu; durum kodu: " << statusCode;
            error = durum.str();
        }
        goto cleanup;
    }

    if (statusCode == 200 && existing > 0) existing = 0;

    if (sceHttpGetResponseContentLength(requestId, &contentLengthType, &responseLength) >= 0) {
        total = existing + static_cast<uint64_t>(responseLength);
    }

    output = std::fopen(destination.c_str(), existing > 0 ? "ab" : "wb");
    if (!output) {
        error = PSF_HEDEF;
        goto cleanup;
    }

    downloaded = existing;
    if (progress) progress(HttpProgress{downloaded, total});

    while (true) {
        if (cancel && cancel->load()) {
            error = PSF_IPTAL;
            sceHttpAbortRequest(requestId);
            goto cleanup;
        }

        const int read = sceHttpReadData(requestId, &buffer[0], static_cast<uint32_t>(buffer.size()));
        if (read < 0) {
            error = PSF_AG_OKUMA;
            goto cleanup;
        }
        if (read == 0) break;

        const size_t written = std::fwrite(&buffer[0], 1, static_cast<size_t>(read), output);
        if (written != static_cast<size_t>(read)) {
            error = PSF_YAZMA;
            goto cleanup;
        }

        downloaded += written;
        if (progress) progress(HttpProgress{downloaded, total});
    }

    if (std::fflush(output) != 0) {
        error = PSF_YAZMA;
        goto cleanup;
    }
    success = true;

cleanup:
    if (output) std::fclose(output);
    if (requestId > 0) sceHttpDeleteRequest(requestId);
    if (connectionId > 0) sceHttpDeleteConnection(connectionId);
    if (templateId > 0) sceHttpDeleteTemplate(templateId);
    return success;
#else
    if (url.compare(0, 7, "file://") != 0) {
        error = PSF_YEREL;
        return false;
    }

    const std::string sourcePath = url.substr(7);
    FILE* input = std::fopen(sourcePath.c_str(), "rb");
    if (!input) {
        error = PSF_KAYNAK;
        return false;
    }

    uint64_t existing = resume ? fileSize(destination) : 0;
    const uint64_t total = fileSize(sourcePath);
    if (existing > total) existing = 0;

    if (existing > 0) std::fseek(input, static_cast<long>(existing), SEEK_SET);
    FILE* output = std::fopen(destination.c_str(), existing > 0 ? "ab" : "wb");
    if (!output) {
        std::fclose(input);
        error = PSF_HEDEF;
        return false;
    }

    uint64_t copied = existing;
    std::vector<uint8_t> buffer(64 * 1024);
    bool success = true;
    if (progress) progress(HttpProgress{copied, total});

    while (true) {
        if (cancel && cancel->load()) {
            error = PSF_IPTAL;
            success = false;
            break;
        }

        const size_t read = std::fread(&buffer[0], 1, buffer.size(), input);
        if (read > 0 && std::fwrite(&buffer[0], 1, read, output) != read) {
            error = PSF_YAZMA;
            success = false;
            break;
        }

        copied += read;
        if (progress) progress(HttpProgress{copied, total});
        if (read < buffer.size()) break;
    }

    std::fclose(output);
    std::fclose(input);
    return success;
#endif
}

}  // namespace psforcer
