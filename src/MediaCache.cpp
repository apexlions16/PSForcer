#include "MediaCache.h"
#include "FileUtil.h"
#include "Sha256.h"

#include <cstdio>
#include <dirent.h>

namespace psforcer {

namespace {
bool terminalState(DownloadState state) {
    return state == DownloadState::Completed ||
           state == DownloadState::Failed ||
           state == DownloadState::Cancelled;
}
}

MediaCache::MediaCache() : nextJobId_(1), initialized_(false) {}
MediaCache::~MediaCache() { shutdown(); }

bool MediaCache::initialize(const std::string& root, std::string& error) {
    root_ = root;
    if (!ensureDirectory(root_)) {
        error = "Geçici görsel klasörü oluşturulamadı";
        return false;
    }
    clearRoot();
    initialized_ = true;
    return true;
}

void MediaCache::shutdown() {
    downloads_.stopAndWait();
    activeUrl_.clear();
    queue_.clear();
    entries_.clear();
    if (initialized_) clearRoot();
    initialized_ = false;
}

bool MediaCache::isRemote(const std::string& value) {
    return value.compare(0, 7, "http://") == 0 || value.compare(0, 8, "https://") == 0;
}

std::string MediaCache::extensionFor(const std::string& url) {
    const size_t query = url.find_first_of("?#");
    const std::string clean = url.substr(0, query);
    const size_t slash = clean.find_last_of('/');
    const size_t dot = clean.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        const std::string extension = clean.substr(dot);
        if (extension == ".jpg" || extension == ".jpeg" || extension == ".png" ||
            extension == ".JPG" || extension == ".JPEG" || extension == ".PNG") {
            return extension;
        }
    }
    return ".img";
}

void MediaCache::clearRoot() {
    if (root_.empty()) return;
    DIR* directory = opendir(root_.c_str());
    if (!directory) return;
    struct dirent* item = NULL;
    while ((item = readdir(directory)) != NULL) {
        const std::string name = item->d_name;
        if (name == "." || name == "..") continue;
        std::remove((root_ + "/" + name).c_str());
    }
    closedir(directory);
}

std::string MediaCache::resolve(const std::string& pathOrUrl) {
    if (!isRemote(pathOrUrl)) return pathOrUrl;
    if (!initialized_) return std::string();

    std::map<std::string, Entry>::iterator found = entries_.find(pathOrUrl);
    if (found == entries_.end()) {
        Sha256 hash;
        hash.update(pathOrUrl);
        Entry entry;
        entry.finalPath = root_ + "/" + hash.finalHex() + extensionFor(pathOrUrl);
        entry.partPath = entry.finalPath + ".parca";
        entries_[pathOrUrl] = entry;
        queue_.push_back(pathOrUrl);
        return std::string();
    }

    if (found->second.state == EntryState::Ready && fileExists(found->second.finalPath)) {
        return found->second.finalPath;
    }
    return std::string();
}

void MediaCache::update() {
    if (!initialized_) return;

    if (!activeUrl_.empty()) {
        const DownloadSnapshot snapshot = downloads_.snapshot();
        if (terminalState(snapshot.state)) {
            Entry& entry = entries_[activeUrl_];
            if (snapshot.state == DownloadState::Completed) {
                std::remove(entry.finalPath.c_str());
                if (std::rename(entry.partPath.c_str(), entry.finalPath.c_str()) == 0) {
                    entry.state = EntryState::Ready;
                } else {
                    entry.state = EntryState::Failed;
                    lastError_ = "Görsel dosyası etkinleştirilemedi";
                }
            } else {
                entry.state = EntryState::Failed;
                lastError_ = snapshot.error;
                std::remove(entry.partPath.c_str());
            }
            downloads_.reset();
            activeUrl_.clear();
        }
    }

    while (activeUrl_.empty() && !queue_.empty()) {
        const std::string url = queue_.front();
        queue_.pop_front();
        Entry& entry = entries_[url];
        if (entry.state != EntryState::Queued) continue;

        DownloadRequest request;
        request.jobId = nextJobId_++;
        request.id = "gorsel";
        request.label = "Görsel hazırlanıyor";
        request.url = url;
        request.destination = entry.partPath;
        request.resume = false;

        std::string error;
        if (!downloads_.start(request, error)) {
            entry.state = EntryState::Failed;
            lastError_ = error;
            continue;
        }
        entry.state = EntryState::Downloading;
        activeUrl_ = url;
    }
}

const std::string& MediaCache::lastError() const { return lastError_; }

}  // namespace psforcer
