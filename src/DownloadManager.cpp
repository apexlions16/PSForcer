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
           message.find("klasör") != std::string::npos;
}

void waitBeforeReconnect(unsigned int attempt) {
    const unsigned int milliseconds = std::min(4000u, 500u + attempt * 250u);
#if defined(PSFORCER_ORBIS)
    SDL_Delay(milliseconds);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
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
    unsigned int reconnectCount = 0;
    unsigned int noProgressCount = 0;
    bool complete = false;

    // Büyük Hugging Face dosyalarında CDN bağlantısı bazen dosyanın tamamı gelmeden
    // normal EOF döndürüyor. Boyut tamamlanana kadar Range ile aynı .parca dosyasını
    // otomatik sürdür; kullanıcının Kare'ye tekrar tekrar basması gerekmesin.
    while (!cancelRequested_.load()) {
        error.clear();
        const bool requestFinished = client_.download(
            request.url,
            request.destination,
            true,
            [this, request](const HttpProgress& progress) {
                std::lock_guard<std::mutex> lock(mutex_);
                snapshot_.downloaded = progress.downloaded;
                snapshot_.total = request.expectedSize > 0
                    ? request.expectedSize
                    : progress.total;
                snapshot_.error.clear();
            },
            &cancelRequested_,
            error);

        const uint64_t actualSize = fileSize(request.destination);
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

        if (!requestFinished && isPermanentDownloadError(error)) {
            lastError = error;
            break;
        }

        if (actualSize > previousSize) {
            previousSize = actualSize;
            noProgressCount = 0;
        } else {
            ++noProgressCount;
        }
        ++reconnectCount;

        if (!error.empty()) lastError = error;

        if (noProgressCount >= 5) {
            std::ostringstream message;
            message << "İndirme ilerlemedi. Sunucu kaldığı yerden devam isteğini "
                    << "kabul etmemiş olabilir; dosya " << actualSize << " baytta kaldı";
            lastError = message.str();
            break;
        }
        if (reconnectCount >= 512) {
            lastError = "İndirme çok fazla yeniden bağlantı denemesine ulaştı";
            break;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::ostringstream message;
            message << "Bağlantı yenileniyor; " << actualSize << " bayt alındı";
            snapshot_.error = message.str();
        }
        waitBeforeReconnect(reconnectCount);
    }

    if (cancelRequested_.load()) {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.state = DownloadState::Cancelled;
        snapshot_.error = "İndirme iptal edildi";
        return;
    }

    if (!complete) {
        if (lastError.empty()) lastError = error.empty() ? "İndirme tamamlanamadı" : error;
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.state = DownloadState::Failed;
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
            snapshot_.error = message.str();
            return;
        }
    }

    if (!request.sha256.empty()) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_.state = DownloadState::Verifying;
            snapshot_.error.clear();
        }
        std::string digest;
        if (!Sha256::fileHex(request.destination, digest, error)) {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_.state = DownloadState::Failed;
            snapshot_.error = error;
            return;
        }
        if (lower(digest) != lower(request.sha256)) {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_.state = DownloadState::Failed;
            snapshot_.error = "SHA-256 bütünlük denetimi başarısız";
            return;
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.downloaded = fileSize(request.destination);
    if (snapshot_.total == 0) snapshot_.total = snapshot_.downloaded;
    snapshot_.state = DownloadState::Completed;
    snapshot_.error.clear();
}

}  // namespace psforcer
