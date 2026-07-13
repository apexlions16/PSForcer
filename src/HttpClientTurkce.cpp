#include "HttpClient.h"
#include "FileUtil.h"
#include "HttpMessages.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sstream>
#include <sys/stat.h>
#include <vector>

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
    if (sceHttpGetAllResponseHeaders(requestId, &raw, &rawSize) < 0 ||
        !raw || rawSize == 0) {
        return std::string();
    }

    const std::string headers(raw, rawSize);
    const std::string wanted = lowerAscii(wantedName);
    size_t cursor = 0;
    while (cursor < headers.size()) {
        const size_t end = headers.find('\n', cursor);
        const size_t lineEnd = end == std::string::npos ? headers.size() : end;
        std::string line = headers.substr(cursor, lineEnd - cursor);
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }
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

std::string absoluteRedirectUrl(const std::string& current,
                                const std::string& location) {
    if (location.compare(0, 7, "http://") == 0 ||
        location.compare(0, 8, "https://") == 0) {
        return location;
    }

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
    if (lastSlash == std::string::npos || lastSlash < hostStart) {
        return origin + "/" + location;
    }
    return current.substr(0, lastSlash + 1) + location;
}

struct ContentRangeInfo {
    bool valid;
    uint64_t start;
    uint64_t end;
    uint64_t total;
    ContentRangeInfo() : valid(false), start(0), end(0), total(0) {}
};

ContentRangeInfo parseContentRange(const std::string& value) {
    ContentRangeInfo info;
    const size_t space = value.find(' ');
    const size_t dash = value.find('-', space == std::string::npos ? 0 : space + 1);
    const size_t slash = value.find('/', dash == std::string::npos ? 0 : dash + 1);
    if (dash == std::string::npos || slash == std::string::npos) return info;

    const size_t startPos = space == std::string::npos ? 0 : space + 1;
    if (startPos >= dash || dash + 1 >= slash) return info;

    info.start = static_cast<uint64_t>(
        std::strtoull(value.substr(startPos, dash - startPos).c_str(), NULL, 10));
    info.end = static_cast<uint64_t>(
        std::strtoull(value.substr(dash + 1, slash - dash - 1).c_str(), NULL, 10));
    if (slash + 1 < value.size() && value[slash + 1] != '*') {
        info.total = static_cast<uint64_t>(
            std::strtoull(value.substr(slash + 1).c_str(), NULL, 10));
    }
    info.valid = info.end >= info.start;
    return info;
}

void closeRequestObjects(int& requestId, int& connectionId) {
    if (requestId > 0) sceHttpDeleteRequest(requestId);
    if (connectionId > 0) sceHttpDeleteConnection(connectionId);
    requestId = 0;
    connectionId = 0;
}

void appendDownloadDiagnostic(uint64_t requestedStart,
                              int32_t statusCode,
                              const ContentRangeInfo& range,
                              uint64_t responseBytes,
                              uint64_t persisted,
                              bool success,
                              const std::string& error) {
    const char* path = "/data/psforcer/indirme_tani.log";
    FILE* log = std::fopen(path, fileSize(path) > 512 * 1024 ? "wb" : "ab");
    if (!log) return;
    std::fprintf(log,
                 "baslangic=%llu durum=%d aralik=%llu-%llu yanit=%llu dosya=%llu sonuc=%s hata=%s\n",
                 static_cast<unsigned long long>(requestedStart),
                 static_cast<int>(statusCode),
                 static_cast<unsigned long long>(range.start),
                 static_cast<unsigned long long>(range.end),
                 static_cast<unsigned long long>(responseBytes),
                 static_cast<unsigned long long>(persisted),
                 success ? "basarili" : "basarisiz",
                 error.empty() ? "-" : error.c_str());
    std::fclose(log);
}
}  // namespace
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
    httpContextId_ = 0;
    sslContextId_ = 0;
    netPoolId_ = 0;
#endif
    initialized_ = false;
}

bool HttpClient::download(const std::string& url,
                          const std::string& destination,
                          bool resume,
                          uint64_t expectedSize,
                          const HttpProgressCallback& progress,
                          std::atomic<bool>* cancel,
                          std::string& error) {
    if (!initialized_ && !initialize(error)) return false;

    const size_t slash = destination.find_last_of('/');
    if (slash != std::string::npos &&
        !ensureDirectory(destination.substr(0, slash))) {
        error = PSF_KLASOR;
        return false;
    }

#if defined(PSFORCER_ORBIS)
    FILE* output = NULL;
    bool success = false;
    uint64_t downloaded = resume ? fileSize(destination) : 0;
    uint64_t total = expectedSize;
    unsigned int reconnectCount = 0;
    unsigned int noProgressCount = 0;
    std::string lastTransientError;

    output = std::fopen(destination.c_str(), downloaded > 0 ? "ab" : "wb");
    if (!output) {
        error = PSF_HEDEF;
        return false;
    }

    std::setvbuf(output, NULL, _IONBF, 0);
    if (progress) progress(HttpProgress{downloaded, total});

    while (!cancel || !cancel->load()) {
        if (expectedSize > 0 && downloaded >= expectedSize) {
            success = downloaded == expectedSize;
            if (!success) error = "Sunucu katalogda belirtilen dosya boyutundan fazla veri gönderdi";
            break;
        }

        const uint64_t requestStart = downloaded;
        uint64_t responseBytes = 0;
        uint64_t expectedResponseBytes = 0;
        int32_t statusCode = 0;
        int contentLengthType = 0;
        uint64_t responseLength = 0;
        ContentRangeInfo contentRange;
        bool rangeRequested = requestStart > 0;
        bool allowToken = true;
        int authFallbackCount = 0;
        int redirectCount = 0;
        std::string currentUrl = url;
        int templateId = 0;
        int connectionId = 0;
        int requestId = 0;

        templateId = sceHttpCreateTemplate(httpContextId_, "PSForcer/0.2.11",
                                           ORBIS_HTTP_VERSION_1_1, 1);
        if (templateId < 0) {
            error = PSF_SABLON;
            break;
        }

        bool requestReady = false;
        while (!requestReady) {
            connectionId = sceHttpCreateConnectionWithURL(templateId,
                                                           currentUrl.c_str(), true);
            if (connectionId < 0) {
                lastTransientError = PSF_BAGLANTI;
                break;
            }

            requestId = sceHttpCreateRequestWithURL(connectionId, ORBIS_METHOD_GET,
                                                    currentUrl.c_str(), 0);
            if (requestId < 0) {
                lastTransientError = PSF_ISTEK;
                break;
            }

            psfHttpSetAutoRedirect(requestId, 0);
            psfHttpSetRecvBlockSize(requestId, 512 * 1024);
            psfHttpSetRecvTimeOut(requestId, 60 * 1000 * 1000);
            sceHttpSetResolveTimeOut(requestId, 20 * 1000 * 1000);
            sceHttpSetConnectTimeOut(requestId, 30 * 1000 * 1000);
            sceHttpSetSendTimeOut(requestId, 30 * 1000 * 1000);
            sceHttpAddRequestHeader(requestId, "Accept", "application/octet-stream", 0);
            sceHttpAddRequestHeader(requestId, "Accept-Encoding", "identity", 0);
            // Her HTTP yanıtını açıkça kapat. Bağlantı erken biterse bir sonraki
            // istek kalıcı dosya ofsetinden doğrulanmış bir Range ile açılır.
            sceHttpAddRequestHeader(requestId, "Connection", "close", 0);

            bool tokenAdded = false;
            if (allowToken && isHuggingFaceUrl(currentUrl)) {
                const std::string token = huggingFaceToken();
                if (!token.empty()) {
                    const std::string authorization = "Bearer " + token;
                    sceHttpAddRequestHeader(requestId, "Authorization",
                                            authorization.c_str(), 0);
                    tokenAdded = true;
                }
            }

            if (rangeRequested) {
                std::ostringstream range;
                range << "bytes=" << requestStart << '-';
                if (expectedSize > 0) range << (expectedSize - 1);
                if (sceHttpAddRequestHeader(requestId, "Range",
                                            range.str().c_str(), 0) < 0) {
                    error = "Kaldığı yerden devam başlığı eklenemedi";
                    break;
                }
            }

            if (sceHttpSendRequest(requestId, NULL, 0) < 0) {
                lastTransientError = PSF_GONDER;
                break;
            }
            if (sceHttpGetStatusCode(requestId, &statusCode) < 0) {
                lastTransientError = PSF_DURUM;
                break;
            }

            if (statusCode == 401 && tokenAdded && authFallbackCount == 0) {
                ++authFallbackCount;
                allowToken = false;
                closeRequestObjects(requestId, connectionId);
                continue;
            }

            if (isRedirectStatus(statusCode)) {
                if (++redirectCount > 8) {
                    error = "İndirme yönlendirme sınırını aştı";
                    break;
                }
                const std::string location = responseHeaderValue(requestId, "Location");
                if (location.empty()) {
                    error = "İndirme yönlendirmesinde hedef adres bulunamadı";
                    break;
                }
                currentUrl = absoluteRedirectUrl(currentUrl, location);
                closeRequestObjects(requestId, connectionId);
                continue;
            }
            requestReady = true;
        }

        if (!requestReady) {
            closeRequestObjects(requestId, connectionId);
            if (templateId > 0) sceHttpDeleteTemplate(templateId);
            if (!error.empty()) break;
            ++noProgressCount;
            ++reconnectCount;
            if (noProgressCount >= 5) {
                error = lastTransientError.empty() ? "İndirme bağlantısı kurulamadı" : lastTransientError;
                break;
            }
            continue;
        }

        if (statusCode == 416 && expectedSize > 0 && requestStart == expectedSize) {
            success = true;
            closeRequestObjects(requestId, connectionId);
            sceHttpDeleteTemplate(templateId);
            break;
        }
        if (statusCode != 200 && statusCode != 206) {
            std::ostringstream message;
            if (statusCode == 401 && isHuggingFaceUrl(currentUrl)) {
                message << "Hugging Face erişimi reddedildi. Tokenı "
                        << "/data/psforcer/hf_token.txt dosyasının ilk satırına yazın";
            } else {
                message << "İndirme isteği başarısız oldu; durum kodu: " << statusCode;
            }
            error = message.str();
            closeRequestObjects(requestId, connectionId);
            sceHttpDeleteTemplate(templateId);
            break;
        }

        contentRange = parseContentRange(responseHeaderValue(requestId, "Content-Range"));
        if (rangeRequested) {
            if (statusCode != 206 || !contentRange.valid || contentRange.start != requestStart) {
                std::ostringstream message;
                message << "Sunucu devam aralığını doğrulamadı. Beklenen başlangıç "
                        << requestStart;
                error = message.str();
                closeRequestObjects(requestId, connectionId);
                sceHttpDeleteTemplate(templateId);
                break;
            }
        }

        if (sceHttpGetResponseContentLength(requestId, &contentLengthType,
                                             &responseLength) >= 0 &&
            contentLengthType == ORBIS_HTTP_CONTENTLEN_EXIST) {
            expectedResponseBytes = responseLength;
            if (total == 0) total = requestStart + responseLength;
        }
        if (contentRange.valid) {
            expectedResponseBytes = contentRange.end - contentRange.start + 1;
            if (contentRange.total > 0) total = contentRange.total;
        }
        if (expectedSize > 0) total = expectedSize;

        // Yanlış nesne veya hata gövdesi diske yazılmadan önce sunucunun
        // bildirdiği tam nesne boyutunu katalogdaki PKG boyutuyla eşleştir.
        if (expectedSize > 0) {
            bool sizeMismatch = false;
            uint64_t reportedSize = 0;
            if (contentRange.valid && contentRange.total > 0) {
                reportedSize = contentRange.total;
                sizeMismatch = reportedSize != expectedSize;
            } else if (!rangeRequested && expectedResponseBytes > 0) {
                reportedSize = expectedResponseBytes;
                sizeMismatch = reportedSize != expectedSize;
            }
            if (sizeMismatch) {
                std::ostringstream message;
                message << "Sunucu dosya boyutu katalogla uyuşmuyor. Beklenen "
                        << expectedSize << " bayt, bildirilen " << reportedSize;
                error = message.str();
                closeRequestObjects(requestId, connectionId);
                sceHttpDeleteTemplate(templateId);
                break;
            }
        }

        std::vector<uint8_t> buffer(512 * 1024);
        bool readFailed = false;
        while (true) {
            // Tam boyuta ulaşıldığında keep-alive EOF'sini bekleme. Ayrıca
            // sceHttpReadData'ya yalnızca dosyada/yanıtta kalan bayt sayısını
            // ver; böylece son küçük parça için 512 KiB tamponun dolması beklenmez.
            if (expectedSize > 0 && downloaded == expectedSize) {
                success = true;
                break;
            }
            if (expectedResponseBytes > 0 &&
                responseBytes == expectedResponseBytes) {
                break;
            }

            if (cancel && cancel->load()) {
                error = PSF_IPTAL;
                sceHttpAbortRequest(requestId);
                readFailed = true;
                break;
            }

            uint64_t readCapacity = buffer.size();
            if (expectedSize > 0) {
                readCapacity = std::min<uint64_t>(
                    readCapacity, expectedSize - downloaded);
            }
            if (expectedResponseBytes > 0) {
                readCapacity = std::min<uint64_t>(
                    readCapacity, expectedResponseBytes - responseBytes);
            }
            if (readCapacity == 0) break;

            const int read = sceHttpReadData(
                requestId, &buffer[0], static_cast<uint32_t>(readCapacity));
            if (read < 0) {
                int32_t lastErrno = 0;
                sceHttpGetLastErrno(requestId, &lastErrno);
                std::ostringstream message;
                message << PSF_AG_OKUMA << " (" << lastErrno << ')';
                lastTransientError = message.str();
                readFailed = true;
                break;
            }
            if (read == 0) break;

            const uint64_t readSize = static_cast<uint64_t>(read);
            if (expectedResponseBytes > 0 &&
                responseBytes + readSize > expectedResponseBytes) {
                error = "Sunucu bildirilen yanıttan fazla veri gönderdi";
                readFailed = true;
                break;
            }
            if (expectedSize > 0 && downloaded + readSize > expectedSize) {
                error = "Sunucu katalogda belirtilen dosya boyutundan fazla veri gönderdi";
                readFailed = true;
                break;
            }

            size_t offset = 0;
            while (offset < static_cast<size_t>(read)) {
                const size_t written = std::fwrite(&buffer[offset], 1,
                                                   static_cast<size_t>(read) - offset,
                                                   output);
                if (written == 0) {
                    error = PSF_YAZMA;
                    readFailed = true;
                    break;
                }
                offset += written;
            }
            if (readFailed) break;

            downloaded += readSize;
            responseBytes += readSize;
            if (progress) progress(HttpProgress{downloaded, total});
        }

        if (std::fflush(output) != 0) {
            error = PSF_YAZMA;
            readFailed = true;
        }

        closeRequestObjects(requestId, connectionId);
        if (templateId > 0) sceHttpDeleteTemplate(templateId);

        if (cancel && cancel->load()) break;
        if (!error.empty() && error != PSF_IPTAL) break;

        if (downloaded > requestStart) {
            noProgressCount = 0;
            ++reconnectCount;
            if (expectedSize > 0 && downloaded == expectedSize) {
                success = true;
                break;
            }
            if (expectedSize == 0 && !readFailed &&
                (expectedResponseBytes == 0 || responseBytes == expectedResponseBytes)) {
                success = true;
                break;
            }
            continue;
        }

        ++noProgressCount;
        ++reconnectCount;
        if (noProgressCount >= 5 || reconnectCount >= 512) {
            error = lastTransientError.empty()
                ? "İndirme ilerlemeden tekrarlandı"
                : lastTransientError;
            break;
        }
    }

    if (cancel && cancel->load() && error.empty()) error = PSF_IPTAL;
    if (std::fflush(output) != 0 && error.empty()) error = PSF_YAZMA;
    if (std::fclose(output) != 0 && error.empty()) error = PSF_YAZMA;
    output = NULL;

    const uint64_t persisted = fileSize(destination);
    if (persisted < downloaded && error.empty()) {
        std::ostringstream message;
        message << "Dosya kapatıldıktan sonra boyut doğrulanamadı. Yazılan "
                << downloaded << " bayt, görünen " << persisted << " bayt";
        error = message.str();
    }
    if (expectedSize > 0 && persisted == expectedSize) success = true;
    if (!success && error.empty()) error = "İndirme tamamlanamadı";

    appendDownloadDiagnostic(0, success ? 200 : 0, ContentRangeInfo(),
                             downloaded, persisted, success, error);
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
    const uint64_t sourceSize = fileSize(sourcePath);
    const uint64_t total = expectedSize > 0 ? expectedSize : sourceSize;
    if (existing > total) existing = 0;

    if (existing > 0) {
        std::fseek(input, static_cast<long>(existing), SEEK_SET);
    }
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

        if (expectedSize > 0 && copied >= expectedSize) break;

        size_t wanted = buffer.size();
        if (expectedSize > 0) {
            wanted = static_cast<size_t>(std::min<uint64_t>(
                wanted, expectedSize - copied));
        }
        const size_t read = std::fread(&buffer[0], 1, wanted, input);
        if (read > 0 && std::fwrite(&buffer[0], 1, read, output) != read) {
            error = PSF_YAZMA;
            success = false;
            break;
        }

        copied += read;
        if (progress) progress(HttpProgress{copied, total});
        if (expectedSize > 0 && copied == expectedSize) break;
        if (read < wanted) break;
    }

    std::fflush(output);
    std::fclose(output);
    std::fclose(input);
    return success;
#endif
}

bool HttpClient::resolvePackageHeader(const std::string& url,
                                      uint64_t expectedSize,
                                      std::string& resolvedUrl,
                                      std::vector<uint8_t>& header,
                                      std::string& error) {
    const size_t headerBytes = 0x438;
    resolvedUrl.clear();
    header.clear();
    if (!initialized_ && !initialize(error)) return false;

#if defined(PSFORCER_ORBIS)
    std::string currentUrl = url;
    const std::string token = huggingFaceToken();
    int redirectCount = 0;

    while (true) {
        int templateId = sceHttpCreateTemplate(httpContextId_, "PSForcer/0.2.11",
                                               ORBIS_HTTP_VERSION_1_1, 1);
        if (templateId < 0) {
            error = PSF_SABLON;
            return false;
        }

        int connectionId = sceHttpCreateConnectionWithURL(templateId,
                                                           currentUrl.c_str(), true);
        if (connectionId < 0) {
            sceHttpDeleteTemplate(templateId);
            error = PSF_BAGLANTI;
            return false;
        }

        int requestId = sceHttpCreateRequestWithURL(connectionId, ORBIS_METHOD_GET,
                                                    currentUrl.c_str(), 0);
        if (requestId < 0) {
            closeRequestObjects(requestId, connectionId);
            sceHttpDeleteTemplate(templateId);
            error = PSF_ISTEK;
            return false;
        }

        psfHttpSetAutoRedirect(requestId, 0);
        psfHttpSetRecvBlockSize(requestId, static_cast<uint32_t>(headerBytes));
        psfHttpSetRecvTimeOut(requestId, 60 * 1000 * 1000);
        sceHttpSetResolveTimeOut(requestId, 20 * 1000 * 1000);
        sceHttpSetConnectTimeOut(requestId, 30 * 1000 * 1000);
        sceHttpSetSendTimeOut(requestId, 30 * 1000 * 1000);
        sceHttpAddRequestHeader(requestId, "Accept", "application/octet-stream", 0);
        sceHttpAddRequestHeader(requestId, "Accept-Encoding", "identity", 0);
        sceHttpAddRequestHeader(requestId, "Connection", "close", 0);
        std::ostringstream range;
        range << "bytes=0-" << (headerBytes - 1);
        sceHttpAddRequestHeader(requestId, "Range", range.str().c_str(), 0);

        if (isHuggingFaceUrl(currentUrl) && !token.empty()) {
            const std::string authorization = "Bearer " + token;
            sceHttpAddRequestHeader(requestId, "Authorization",
                                    authorization.c_str(), 0);
        }

        if (sceHttpSendRequest(requestId, NULL, 0) < 0) {
            closeRequestObjects(requestId, connectionId);
            sceHttpDeleteTemplate(templateId);
            error = PSF_GONDER;
            return false;
        }

        int32_t statusCode = 0;
        if (sceHttpGetStatusCode(requestId, &statusCode) < 0) {
            closeRequestObjects(requestId, connectionId);
            sceHttpDeleteTemplate(templateId);
            error = PSF_DURUM;
            return false;
        }

        if (isRedirectStatus(statusCode)) {
            if (++redirectCount > 8) {
                closeRequestObjects(requestId, connectionId);
                sceHttpDeleteTemplate(templateId);
                error = "PKG adresi yönlendirme sınırını aştı";
                return false;
            }
            const std::string location = responseHeaderValue(requestId, "Location");
            if (location.empty()) {
                closeRequestObjects(requestId, connectionId);
                sceHttpDeleteTemplate(templateId);
                error = "PKG yönlendirmesinde hedef adres bulunamadı";
                return false;
            }
            currentUrl = absoluteRedirectUrl(currentUrl, location);
            closeRequestObjects(requestId, connectionId);
            sceHttpDeleteTemplate(templateId);
            continue;
        }

        if (statusCode == 401 && isHuggingFaceUrl(currentUrl)) {
            closeRequestObjects(requestId, connectionId);
            sceHttpDeleteTemplate(templateId);
            error = "Hugging Face erişimi reddedildi. Salt-okunur tokenı "
                    "/data/psforcer/hf_token.txt dosyasının ilk satırına yazın";
            return false;
        }
        if (statusCode != 200 && statusCode != 206) {
            closeRequestObjects(requestId, connectionId);
            sceHttpDeleteTemplate(templateId);
            std::ostringstream message;
            message << "PKG başlığı alınamadı; HTTP " << statusCode;
            error = message.str();
            return false;
        }

        uint64_t reportedSize = 0;
        const ContentRangeInfo contentRange =
            parseContentRange(responseHeaderValue(requestId, "Content-Range"));
        if (contentRange.valid && contentRange.total > 0) {
            reportedSize = contentRange.total;
        } else {
            int contentLengthType = 0;
            uint64_t responseLength = 0;
            if (statusCode == 200 &&
                sceHttpGetResponseContentLength(requestId, &contentLengthType,
                                                 &responseLength) >= 0 &&
                contentLengthType == ORBIS_HTTP_CONTENTLEN_EXIST) {
                reportedSize = responseLength;
            }
        }
        if (expectedSize > 0 && reportedSize > 0 && reportedSize != expectedSize) {
            closeRequestObjects(requestId, connectionId);
            sceHttpDeleteTemplate(templateId);
            std::ostringstream message;
            message << "Uzak PKG boyutu katalogla uyuşmuyor. Beklenen "
                    << expectedSize << " bayt, bildirilen " << reportedSize;
            error = message.str();
            return false;
        }

        header.assign(headerBytes, 0);
        size_t received = 0;
        while (received < headerBytes) {
            const int read = sceHttpReadData(
                requestId, &header[received],
                static_cast<uint32_t>(headerBytes - received));
            if (read <= 0) {
                closeRequestObjects(requestId, connectionId);
                sceHttpDeleteTemplate(templateId);
                header.clear();
                error = read < 0 ? PSF_AG_OKUMA : "PKG başlığı erken sona erdi";
                return false;
            }
            received += static_cast<size_t>(read);
        }

        resolvedUrl = currentUrl;
        closeRequestObjects(requestId, connectionId);
        sceHttpDeleteTemplate(templateId);
        error.clear();
        return true;
    }
#else
    if (url.compare(0, 7, "file://") != 0) {
        error = PSF_YEREL;
        return false;
    }
    const std::string path = url.substr(7);
    if (expectedSize > 0 && fileSize(path) != expectedSize) {
        error = "Yerel PKG boyutu katalogla uyuşmuyor";
        return false;
    }
    FILE* input = std::fopen(path.c_str(), "rb");
    if (!input) {
        error = PSF_KAYNAK;
        return false;
    }
    header.assign(headerBytes, 0);
    const size_t received = std::fread(&header[0], 1, headerBytes, input);
    std::fclose(input);
    if (received != headerBytes) {
        header.clear();
        error = "PKG başlığı erken sona erdi";
        return false;
    }
    resolvedUrl = url;
    error.clear();
    return true;
#endif
}

}  // namespace psforcer
