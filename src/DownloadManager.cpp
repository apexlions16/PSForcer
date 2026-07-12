#include "DownloadManager.h"
#include "FileUtil.h"
#include "Sha256.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#if defined(PSFORCER_ORBIS)
#include <SDL2/SDL.h>
#endif

namespace psforcer {

namespace {
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   static_cast<int(*)(int)>(std::tolower));
    return value;
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
    const bool downloaded = client_.download(
        request.url,
        request.destination,
        request.resume,
        [this, request](const HttpProgress& progress) {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_.downloaded = progress.downloaded;
            if (progress.total > 0) snapshot_.total = progress.total;
            else if (snapshot_.total == 0) snapshot_.total = request.expectedSize;
        },
        &cancelRequested_,
        error);

    if (!downloaded) {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.state = cancelRequested_.load() ? DownloadState::Cancelled : DownloadState::Failed;
        snapshot_.error = error;
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
}

}  // namespace psforcer
