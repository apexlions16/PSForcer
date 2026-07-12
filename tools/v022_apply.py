from pathlib import Path

path = Path('src/HttpClientTurkce.cpp')
text = path.read_text()
text = text.replace('#include <fcntl.h>\n', '')
text = text.replace('#include <orbis/libkernel.h>\n', '')

begin = text.index('#if defined(PSFORCER_ORBIS)\n    int templateId = 0;', text.index('bool HttpClient::download'))
end = text.index('#else\n    (void)expectedSize;', begin)

body = r'''#if defined(PSFORCER_ORBIS)
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

        templateId = sceHttpCreateTemplate(httpContextId_, "PSForcer/0.22",
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
            sceHttpAddRequestHeader(requestId, "Connection", "keep-alive", 0);

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
            message << "İndirme isteği başarısız oldu; durum kodu: " << statusCode;
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

        std::vector<uint8_t> buffer(512 * 1024);
        bool readFailed = false;
        while (true) {
            if (cancel && cancel->load()) {
                error = PSF_IPTAL;
                sceHttpAbortRequest(requestId);
                readFailed = true;
                break;
            }

            const int read = sceHttpReadData(requestId, &buffer[0],
                                             static_cast<uint32_t>(buffer.size()));
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
'''

text = text[:begin] + body + text[end:]
path.write_text(text)

make = Path('Makefile')
m = make.read_text().replace('v0.21', 'v0.22').replace('VERSION     := 0.21', 'VERSION     := 0.22')
make.write_text(m)

workflow = Path('.github/workflows/build-pkg-release.yml')
w = workflow.read_text()
w = w.replace('YAYIN_ETIKETI: v0.2.1', 'YAYIN_ETIKETI: v0.2.2')
w = w.replace('ESKI_YAYIN_ETIKETI: v0.2.0', 'ESKI_YAYIN_ETIKETI: v0.2.1')
w = w.replace('PSForcer v0.2.1 Kalıcı İndirme Yazımı', 'PSForcer v0.2.2 Tek Oturumlu İndirme')
old_note = 'İndirilen veri stdio ekleme tamponu yerine mutlak dosya konumuna sceKernelPwrite ile yazılır ve her 8 MiB’de sceKernelFsync ile kalıcılaştırılır. Sabit 64 MiB istek sınırı kaldırılmıştır; erken kapanan bağlantılar gerçek disk boyutundan devam eder. İlerleme sayacı monoton tutulur, dosya boyutu küçülürse döngüye girmek yerine tanı kaydıyla durur.'
new_note = 'PS4 üzerinde güvenilir olmadığı görülen sceKernelPwrite yolu kaldırıldı. İndirme boyunca tek bir standart libc FILE akışı açık tutulur, tampon kapatılır ve bağlantı kesilse bile aynı kesin bellek konumundan Range isteğiyle devam edilir. stat() yalnızca dosya kapatıldıktan sonra doğrulama amacıyla kullanılır; böylece bağlantı yenilemelerinde sayaç ve dosya konumu geriye dönmez.'
w = w.replace(old_note, new_note)
workflow.write_text(w)
