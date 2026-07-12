#include "DownloadManager.h"
#include "FileUtil.h"
#include "Sha256.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#if defined(PSFORCER_ORBIS)
#include <SDL2/SDL.h>
#else
#include <chrono>
#include <thread>
#endif

namespace psforcer {

namespace {
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   static_cast<int(*)(int)>(std::tolower));
    return value;
}

bool isPermanentDownloadError(const std::string& error) {
    const std::string message = lower(error);
    return message.find("401") != std::string::npos ||
           message.find("403") != std::string::npos ||
           message.find("404") != std::string::npos ||
           message.find("hedef dosya") != std::string::npos ||
           message.find("dosyaya yaz") != std::string::npos ||
           message.find("klasör") != std::string::npos ||
           message.find("yanlış aralıktan") != std::string::npos;
}

uint64_t monotonicMilliseconds() {
#if defined(PSFORCER_ORBIS)
    return static_cast<uint64_t>(SDL_GetTicks());
#else
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
}

void waitBeforeReconnect(unsigned int failureCount) {
#if defined(PSFORCER_ORBIS)
    const unsigned int milliseconds = std::min(3000u, 250u + failureCount * 350u);
    SDL_Delay(milliseconds);
#else
    (void)failureCount;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
#endif
}
}

DownloadManager::DownloadManager()
#if defined(PSFORCER_ORBIS)
    : worker_(NULL),
#else
    :
#endif
      cancelRequested_(false) {}

DownloadManager::~DownloadManager() { stopAndWait(); }

void DownloadManager::waitWorker() {
#if defined(PSFORCER_ORBIS)
    if (worker_) {
        SDL_WaitThread(worker_, NULL);
        worker_ = NULL;
    }
#else
    if (worker_.joinable()) worker_.join();
#endif
}

bool DownloadManager::start(const DownloadRequest& request, std::string& error) {
    if (busy()) {
        error = "Başka bir indirme zaten çalışıyor";
        return false;
    }
    if (request.url.empty()) {
        error = "İndirme adresi boş";
        return false;
    }

    waitWorker();

    // Ağ modüllerini ve HTTP bağlamını ana iş parçacığında hazırla.
    // PS4'te modül yükleme ve ilk ağ başlatma işlemlerini yeni oluşturulan
    // iş parçacığının içinde yapmak bazı ortamlarda CE-34878-0 ile sonuçlanabiliyor.
    if (!client_.initialize(error)) return false;

    cancelRequested_.store(false);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_ = DownloadSnapshot();
        snapshot_.jobId = request.jobId;
        snapshot_.state = DownloadState::Running;
        snapshot_.id = request.id;
        snapshot_.label = request.label;
        snapshot_.destination = request.destination;
        snapshot_.downloaded = request.resume ? fileSize(request.destination) : 0;
        snapshot_.total = request.expectedSize;
        snapshot_.status = "İNDİRME HAZIRLANIYOR";
#if defined(PSFORCER_ORBIS)
        pendingRequest_ = request;
#endif
    }

#if defined(PSFORCER_ORBIS)
    worker_ = SDL_CreateThread(&DownloadManager::threadEntry, "PSForcerIndirme", this);
    if (!worker_) {
        error = std::string("İndirme iş parçacığı oluşturulamadı: ") + SDL_GetError();
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.state = DownloadState::Failed;
        snapshot_.error = error;
        snapshot_.status.clear();
        return false;
    }
#else
    worker_ = std::thread(&DownloadManager::run, this, request);
#endif
    return true;
}

#if defined(PSFORCER_ORBIS)
int DownloadManager::threadEntry(void* data) {
    DownloadManager* self = static_cast<DownloadManager*>(data);
    DownloadRequest request;
    {
        std::lock_guard<std::mutex> lock(self->mutex_);
        request = self->pendingRequest_;
    }
    self->run(request);
    return 0;
}
#endif

void DownloadManager::cancel() { cancelRequested_.store(true); }

void DownloadManager::stopAndWait() {
    cancel();
    waitWorker();
}

void DownloadManager::reset() {
    if (busy()) return;
    waitWorker();
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_ = DownloadSnapshot();
}

DownloadSnapshot DownloadManager::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

bool DownloadManager::busy() const {
    const DownloadSnapshot current = snapshot();
    return current.state == DownloadState::Running || current.state == DownloadState::Verifying;
}

void DownloadManager::run(DownloadRequest request) {
    std::string error;
    std::string lastError;
    uint64_t previousSize = request.resume ? fileSize(request.destination) : 0;
    const uint64_t sessionStartSize = previousSize;
    const uint64_t sessionStartTime = monotonicMilliseconds();
    unsigned int reconnectCount = 0;
    unsigned int noProgressCount = 0;
    bool firstAttempt = true;
    bool complete = false;

    // Hugging Face/Xet büyük dosyayı birden fazla HTTP aralığı halinde verebilir.
    // Veri ilerlediyse bu bir hata değildir: gecikmeden sonraki aralığa geçilir.
    while (!cancelRequested_.load()) {
        error.clear();
        const bool requestFinished = client_.download(
            request.url,
            request.destination,
            firstAttempt ? request.resume : true,
            request.expectedSize,
            [this, request, sessionStartSize, sessionStartTime](const HttpProgress& progress) {
                const uint64_t now = monotonicMilliseconds();
                const uint64_t elapsed = now >= sessionStartTime ? now - sessionStartTime : 0;
                const uint64_t sessionBytes = progress.downloaded >= sessionStartSize
                    ? progress.downloaded - sessionStartSize
                    : 0;

                std::lock_guard<std::mutex> lock(mutex_);
                snapshot_.downloaded = progress.downloaded;
                snapshot_.total = request.expectedSize > 0
                    ? request.expectedSize
                    : progress.total;
                if (elapsed >= 250)
                    snapshot_.bytesPerSecond = sessionBytes * 1000ULL / elapsed;
                snapshot_.status.clear();
                snapshot_.error.clear();
            },
            &cancelRequested_,
            error);
        firstAttempt = false;

        const uint64_t actualSize = fileSize(request.destination);
        const bool madeProgress = actualSize > previousSize;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_.downloaded = actualSize;
            if (request.expectedSize > 0) snapshot_.total = request.expectedSize;
        }

        if (cancelRequested_.load()) break;

        if (request.expectedSize > 0) {
            if (actualSize == request.expectedSize) {
                complete = true;
                break;
            }
            if (actualSize > request.expectedSize) {
                std::ostringstream message;
                message << "Sunucu beklenenden büyük veri gönderdi. Beklenen "
                        << request.expectedSize << " bayt, alınan "
                        << actualSize << " bayt";
                lastError = message.str();
                break;
            }
        } else if (requestFinished) {
            complete = true;
            break;
        }

        if (madeProgress) {
            previousSize = actualSize;
            noProgressCount = 0;
            ++reconnectCount;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                snapshot_.reconnects = reconnectCount;
                snapshot_.status.clear();
            }
            // Sağlıklı parça tamamlandı. Eski sürüm burada her defasında 0,5–4 sn
            // beklediği için yüzlerce aralıkta indirme yapay biçimde yavaşlıyordu.
            continue;
        }

        if (!requestFinished && isPermanentDownloadError(error)) {
            lastError = error;
            break;
        }

        ++noProgressCount;
        ++reconnectCount;
        if (!error.empty()) lastError = error;

        if (noProgressCount >= 5) {
            std::ostringstream message;
            message << "Dosya boyutu doğrulanamadı. İndirme ilerlemedi; sunucu "
                    << "kaldığı yerden devam isteğini kabul etmemiş olabilir. Alınan "
                    << actualSize << " bayt";
            lastError = message.str();
            break;
        }
        if (reconnectCount >= 512) {
            lastError = "İndirme çok fazla yeniden bağlantı denemesine ulaştı";
            break;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_.reconnects = reconnectCount;
            snapshot_.status = "BAĞLANTI TEKRAR KURULUYOR";
            snapshot_.error.clear();
        }
        waitBeforeReconnect(noProgressCount);
    }

    if (cancelRequested_.load()) {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.state = DownloadState::Cancelled;
        snapshot_.status.clear();
        snapshot_.error = "İndirme iptal edildi";
        return;
    }

    if (!complete) {
        if (lastError.empty()) lastError = error.empty() ? "İndirme tamamlanamadı" : error;
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.state = DownloadState::Failed;
        snapshot_.status.clear();
        snapshot_.error = lastError;
        return;
    }

    if (request.expectedSize > 0) {
        const uint64_t actualSize = fileSize(request.destination);
        if (actualSize != request.expectedSize) {
            std::ostringstream message;
            message << "Dosya boyutu doğrulanamadı. Beklenen "
                    << request.expectedSize << " bayt, alınan "
                    << actualSize << " bayt";
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_.downloaded = actualSize;
            snapshot_.total = request.expectedSize;
            snapshot_.state = DownloadState::Failed;
            snapshot_.status.clear();
            snapshot_.error = message.str();
            return;
        }
    }

    if (!request.sha256.empty()) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_.state = DownloadState::Verifying;
            snapshot_.status = "BÜTÜNLÜK DENETLENİYOR";
            snapshot_.error.clear();
        }
        std::string digest;
        if (!Sha256::fileHex(request.destination, digest, error)) {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_.state = DownloadState::Failed;
            snapshot_.status.clear();
            snapshot_.error = error;
            return;
        }
        if (lower(digest) != lower(request.sha256)) {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_.state = DownloadState::Failed;
            snapshot_.status.clear();
            snapshot_.error = "SHA-256 bütünlük denetimi başarısız";
            return;
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.downloaded = fileSize(request.destination);
    if (snapshot_.total == 0) snapshot_.total = snapshot_.downloaded;
    snapshot_.state = DownloadState::Completed;
    snapshot_.status.clear();
    snapshot_.error.clear();
}

}  // namespace psforcer
