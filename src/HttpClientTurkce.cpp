#include "HttpClient.h"
#include "FileUtil.h"
#include "HttpMessages.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
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

// OpenOrbis başlığında bu üç işlev eski, parametresiz bildirimlerle yer alıyor.
// Gerçek libSceHttp imzalarına güvenli takma ad veriyoruz.
extern "C" int32_t psfHttpSetAutoRedirect(int32_t id, int32_t enabled)
    __asm__("sceHttpSetAutoRedirect");
extern "C" int32_t psfHttpSetRecvTimeOut(int32_t id, uint32_t usec)
    __asm__("sceHttpSetRecvTimeOut");
extern "C" int32_t psfHttpSetRecvBlockSize(int32_t id, uint32_t size)
    __asm__("sceHttpSetRecvBlockSize");
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
    const std::string token = trim(readFirstLine("/data/psforcer/hf_token.txt"));
    if (token.empty() || token[0] == '#') return std::string();
    return token;
}

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   static_cast<int(*)(int)>(std::tolower));
    return value;
}

bool isRedirectStatus(int status) {
    return status == 301 || status == 302 || status == 303 ||
           status == 307 || status == 308;
}

std::string responseHeaderValue(int requestId, const std::string& wantedName) {
    char* raw = NULL;
    size_t rawSize = 0;
    if (sceHttpGetAllResponseHeaders(requestId, &raw, &rawSize) < 0 || !raw || rawSize == 0)
        return std::string();

    const std::string headers(raw, rawSize);
    const std::string wanted = lowerAscii(wantedName);
    size_t cursor = 0;
    while (cursor < headers.size()) {
        const size_t end = headers.find('\n', cursor);
        const size_t lineEnd = end == std::string::npos ? headers.size() : end;
        std::string line = headers.substr(cursor, lineEnd - cursor);
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        const size_t colon = line.find(':');
        if (colon != std::string::npos) {
            const std::string name = lowerAscii(trim(line.substr(0, colon)));
            if (name == wanted) return trim(line.substr(colon + 1));
        }
        if (end == std::string::npos) break;
        cursor = end + 1;
    }
    return std::string();
}

std::string absoluteRedirectUrl(const std::string& current, const std::string& location) {
    if (location.compare(0, 7, "http://") == 0 || location.compare(0, 8, "https://") == 0)
        return location;

    const size_t schemeEnd = current.find("://");
    if (schemeEnd == std::string::npos) return location;
    const std::string scheme = current.substr(0, schemeEnd);

    if (location.compare(0, 2, "//") == 0) return scheme + ":" + location;

    const size_t hostStart = schemeEnd + 3;
    const size_t pathStart = current.find('/', hostStart);
    const std::string origin = pathStart == std::string::npos
        ? current
        : current.substr(0, pathStart);

    if (!location.empty() && location[0] == '/') return origin + location;

    const size_t lastSlash = current.rfind('/');
    if (lastSlash == std::string::npos || lastSlash < hostStart) return origin + "/" + location;
    return current.substr(0, lastSlash + 1) + location;
}

uint64_t contentRangeTotal(const std::string& value) {
    const size_t slash = value.rfind('/');
    if (slash == std::string::npos || slash + 1 >= value.size() || value[slash + 1] == '*')
        return 0;
    return static_cast<uint64_t>(std::strtoull(value.substr(slash + 1).c_str(), NULL, 10));
}

void closeRequestObjects(int& requestId, int& connectionId) {
    if (requestId > 0) sceHttpDeleteRequest(requestId);
    if (connectionId > 0) sceHttpDeleteConnection(connectionId);
    requestId = 0;
    connectionId = 0;
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

        gNetPoolId = sceNetPoolCreate("PSForcerAg", 2 * 1024 * 1024, 0);
        if (gNetPoolId < 0) {
            error = PSF_AG_HAVUZU;
            gNetPoolId = 0;
            return false;
        }

        gSslContextId = sceSslInit(2 * 1024 * 1024);
        if (gSslContextId < 0) {
            error = PSF_SSL;
            sceNetPoolDestroy(gNetPoolId);
            gNetPoolId = 0;
            gSslContextId = 0;
            return false;
        }

        gHttpContextId = sceHttpInit(gNetPoolId, gSslContextId, 8 * 1024 * 1024);
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
    uint64_t rangeTotal = 0;
    std::string currentUrl = url;
    bool allowToken = true;
    int redirectCount = 0;
    int authFallbackCount = 0;

    // Daha büyük blok, libSceHttp çağrı sayısını düşürür ve PS4 üzerinde
    // tek bağlantılı indirme hızını belirgin biçimde iyileştirir.
    std::vector<uint8_t> buffer(512 * 1024);

    templateId = sceHttpCreateTemplate(httpContextId_, "PSForcer/0.18", ORBIS_HTTP_VERSION_1_1, 1);
    if (templateId < 0) {
        error = PSF_SABLON;
        return false;
    }

    while (true) {
        connectionId = sceHttpCreateConnectionWithURL(templateId, currentUrl.c_str(), true);
        if (connectionId < 0) {
            error = PSF_BAGLANTI;
            goto cleanup;
        }

        requestId = sceHttpCreateRequestWithURL(connectionId, ORBIS_METHOD_GET,
                                                currentUrl.c_str(), 0);
        if (requestId < 0) {
            error = PSF_ISTEK;
            goto cleanup;
        }

        // Yönlendirmeyi kendimiz izliyoruz. Böylece Range başlığı Hugging Face'in
        // imzalı CDN adresine geçerken kaybolmuyor.
        psfHttpSetAutoRedirect(requestId, 0);
        psfHttpSetRecvBlockSize(requestId, 512 * 1024);
        psfHttpSetRecvTimeOut(requestId, 60 * 1000 * 1000);
        sceHttpSetResolveTimeOut(requestId, 20 * 1000 * 1000);
        sceHttpSetConnectTimeOut(requestId, 30 * 1000 * 1000);
        sceHttpSetSendTimeOut(requestId, 30 * 1000 * 1000);

        sceHttpAddRequestHeader(requestId, "Accept", "application/octet-stream", 0);
        sceHttpAddRequestHeader(requestId, "Accept-Encoding", "identity", 0);
        sceHttpAddRequestHeader(requestId, "Connection", "keep-alive", 0);

        bool tokenAdded = false;
        if (allowToken && isHuggingFaceUrl(currentUrl)) {
            const std::string token = huggingFaceToken();
            if (!token.empty()) {
                const std::string authorization = "Bearer " + token;
                sceHttpAddRequestHeader(requestId, "Authorization", authorization.c_str(), 0);
                tokenAdded = true;
            }
        }

        if (existing > 0) {
            std::ostringstream range;
            range << "bytes=" << existing << '-';
            if (sceHttpAddRequestHeader(requestId, "Range", range.str().c_str(), 0) < 0)
                existing = 0;
        }

        if (sceHttpSendRequest(requestId, NULL, 0) < 0) {
            error = PSF_GONDER;
            goto cleanup;
        }

        if (sceHttpGetStatusCode(requestId, &statusCode) < 0) {
            error = PSF_DURUM;
            goto cleanup;
        }

        // Geçersiz/eski token public bir dosyayı engellemesin: bir kez anonim dene.
        if (statusCode == 401 && tokenAdded && authFallbackCount == 0) {
            ++authFallbackCount;
            allowToken = false;
            closeRequestObjects(requestId, connectionId);
            continue;
        }

        if (isRedirectStatus(statusCode)) {
            if (++redirectCount > 8) {
                error = "İndirme yönlendirme sınırını aştı";
                goto cleanup;
            }
            const std::string location = responseHeaderValue(requestId, "Location");
            if (location.empty()) {
                error = "İndirme yönlendirmesinde hedef adres bulunamadı";
                goto cleanup;
            }
            currentUrl = absoluteRedirectUrl(currentUrl, location);
            closeRequestObjects(requestId, connectionId);
            continue;
        }
        break;
    }

    if (statusCode == 416 && existing > 0) {
        success = true;
        goto cleanup;
    }

    if (statusCode != 200 && statusCode != 206) {
        if (statusCode == 401 && isHuggingFaceUrl(currentUrl)) {
            error = "Hugging Face erişimi reddetti (401): token geçersiz veya depo erişimi kapalı";
        } else if (statusCode == 403) {
            error = "İndirme sunucusu erişimi reddetti (403)";
        } else {
            std::ostringstream durum;
            durum << "İndirme isteği başarısız oldu; durum kodu: " << statusCode;
            error = durum.str();
        }
        goto cleanup;
    }

    // Sunucu Range isteğini yok saydıysa dosyayı baştan, bozmadan yaz.
    if (statusCode == 200 && existing > 0) existing = 0;

    if (sceHttpGetResponseContentLength(requestId, &contentLengthType, &responseLength) >= 0)
        total = existing + static_cast<uint64_t>(responseLength);

    rangeTotal = contentRangeTotal(responseHeaderValue(requestId, "Content-Range"));
    if (rangeTotal > 0) total = rangeTotal;

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

        const int read = sceHttpReadData(requestId, &buffer[0],
                                         static_cast<uint32_t>(buffer.size()));
        if (read < 0) {
            int32_t lastErrno = 0;
            sceHttpGetLastErrno(requestId, &lastErrno);
            std::ostringstream message;
            message << PSF_AG_OKUMA << " (" << lastErrno << ")";
            error = message.str();
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
    closeRequestObjects(requestId, connectionId);
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
    std::vector<uint8_t> buffer(512 * 1024);
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
