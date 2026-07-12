#include "DownloadManager.h"
#include "Sha256.h"

#include <algorithm>
#include <cctype>

namespace psforcer {

namespace {
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   static_cast<int(*)(int)>(std::tolower));
    return value;
}
}

DownloadManager::DownloadManager() : cancelRequested_(false) {}

DownloadManager::~DownloadManager() {
    cancel();
    if (worker_.joinable()) worker_.join();
}

bool DownloadManager::start(const DownloadRequest& request, std::string& error) {
    if (busy()) {
        error = "Another download is already running";
        return false;
    }
    if (request.url.empty()) {
        error = "Download URL is empty";
        return false;
    }
    if (worker_.joinable()) worker_.join();
    cancelRequested_.store(false);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_ = DownloadSnapshot();
        snapshot_.jobId = request.jobId;
        snapshot_.state = DownloadState::Running;
        snapshot_.id = request.id;
        snapshot_.label = request.label;
        snapshot_.destination = request.destination;
    }
    worker_ = std::thread(&DownloadManager::run, this, request);
    return true;
}

void DownloadManager::cancel() {
    cancelRequested_.store(true);
}

void DownloadManager::reset() {
    if (busy()) return;
    if (worker_.joinable()) worker_.join();
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
        [this](const HttpProgress& progress) {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_.downloaded = progress.downloaded;
            snapshot_.total = progress.total;
        },
        &cancelRequested_,
        error);

    if (!downloaded) {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.state = cancelRequested_.load() ? DownloadState::Cancelled : DownloadState::Failed;
        snapshot_.error = error;
        return;
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
            snapshot_.error = "SHA-256 verification failed";
            return;
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.state = DownloadState::Completed;
}

}  // namespace psforcer
