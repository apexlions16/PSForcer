#pragma once

#include <atomic>
#include <functional>
#include <stdint.h>
#include <string>

namespace psforcer {

struct HttpProgress {
    uint64_t downloaded;
    uint64_t total;
};

typedef std::function<void(const HttpProgress&)> HttpProgressCallback;

class HttpClient {
public:
    HttpClient();
    ~HttpClient();
    bool initialize(std::string& error);
    void shutdown();
    bool download(const std::string& url,
                  const std::string& destination,
                  bool resume,
                  const HttpProgressCallback& progress,
                  std::atomic<bool>* cancel,
                  std::string& error);
private:
    bool initialized_;
#if defined(PSFORCER_ORBIS)
    int netPoolId_;
    int sslContextId_;
    int httpContextId_;
#endif
};

}  // namespace psforcer
