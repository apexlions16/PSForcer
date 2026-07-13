#pragma once

#include <atomic>
#include <functional>
#include <stdint.h>
#include <string>
#include <vector>

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
                  uint64_t expectedSize,
                  const HttpProgressCallback& progress,
                  std::atomic<bool>* cancel,
                  std::string& error);
    bool resolvePackageHeader(const std::string& url,
                              uint64_t expectedSize,
                              std::string& resolvedUrl,
                              std::vector<uint8_t>& header,
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
