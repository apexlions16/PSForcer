#pragma once

#include "HttpClient.h"

#include <atomic>
#include <mutex>
#include <string>
#include <stdint.h>

#if defined(PSFORCER_ORBIS)
#include <SDL2/SDL_thread.h>
#else
#include <thread>
#endif

namespace psforcer {

enum class DownloadState { Idle, Running, Verifying, Completed, Failed, Cancelled };

struct DownloadRequest {
    uint64_t jobId;
    std::string id;
    std::string label;
    std::string url;
    std::string destination;
    std::string sha256;
    uint64_t expectedSize;
    bool resume;
    DownloadRequest() : jobId(0), expectedSize(0), resume(true) {}
};

struct DownloadSnapshot {
    uint64_t jobId;
    DownloadState state;
    std::string id;
    std::string label;
    std::string destination;
    uint64_t downloaded;
    uint64_t total;
    std::string error;
    DownloadSnapshot() : jobId(0), state(DownloadState::Idle), downloaded(0), total(0) {}
};

class DownloadManager {
public:
    DownloadManager();
    ~DownloadManager();
    bool start(const DownloadRequest& request, std::string& error);
    void cancel();
    void stopAndWait();
    void reset();
    DownloadSnapshot snapshot() const;
    bool busy() const;
private:
    void run(DownloadRequest request);
    void waitWorker();
#if defined(PSFORCER_ORBIS)
    static int threadEntry(void* data);
    SDL_Thread* worker_;
    DownloadRequest pendingRequest_;
#else
    std::thread worker_;
#endif
    mutable std::mutex mutex_;
    std::atomic<bool> cancelRequested_;
    DownloadSnapshot snapshot_;
    HttpClient client_;
};

}  // namespace psforcer
