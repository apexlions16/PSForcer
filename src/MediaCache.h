#pragma once

#include "DownloadManager.h"

#include <deque>
#include <map>
#include <string>
#include <stdint.h>

namespace psforcer {

class MediaCache {
public:
    MediaCache();
    ~MediaCache();
    bool initialize(const std::string& root, std::string& error);
    void shutdown();
    std::string resolve(const std::string& pathOrUrl);
    void update();
    const std::string& lastError() const;
private:
    enum class EntryState { Queued, Downloading, Ready, Failed };
    struct Entry {
        EntryState state;
        std::string finalPath;
        std::string partPath;
        Entry() : state(EntryState::Queued) {}
    };

    static bool isRemote(const std::string& value);
    static std::string extensionFor(const std::string& url);
    void clearRoot();

    std::string root_;
    std::map<std::string, Entry> entries_;
    std::deque<std::string> queue_;
    DownloadManager downloads_;
    std::string activeUrl_;
    std::string lastError_;
    uint64_t nextJobId_;
    bool initialized_;
};

}  // namespace psforcer
