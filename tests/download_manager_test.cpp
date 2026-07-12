#include "DownloadManager.h"
#include "FileUtil.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

namespace {
bool terminal(psforcer::DownloadState state) {
    return state == psforcer::DownloadState::Completed ||
           state == psforcer::DownloadState::Failed ||
           state == psforcer::DownloadState::Cancelled;
}

psforcer::DownloadSnapshot waitFor(psforcer::DownloadManager& manager,
                                   bool* geriyeSardi = NULL) {
    uint64_t onceki = 0;
    for (int i = 0; i < 500; ++i) {
        psforcer::DownloadSnapshot snapshot = manager.snapshot();
        if (geriyeSardi && snapshot.downloaded < onceki) {
            *geriyeSardi = true;
        }
        if (snapshot.downloaded > onceki) onceki = snapshot.downloaded;
        if (terminal(snapshot.state)) return snapshot;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return manager.snapshot();
}
}

int main() {
    const std::string source = "indirme-kaynak.bin";
    const std::string destination = "indirme-hedef.bin.parca";
    const std::string payload(2 * 1024 * 1024, 'P');
    {
        std::ofstream output(source.c_str(), std::ios::binary);
        output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    }
    {
        // Gerçek PS4 kullanımındaki yarım .parca dosyasını taklit et.
        std::ofstream output(destination.c_str(), std::ios::binary);
        output.write(payload.data(), 64 * 1024);
    }

    psforcer::DownloadManager manager;
    psforcer::DownloadRequest request;
    request.jobId = 1;
    request.id = "sinama";
    request.label = "İndirme sınaması";
    request.url = "file://" + source;
    request.destination = destination;
    request.expectedSize = payload.size();
    request.resume = true;

    std::string error;
    if (!manager.start(request, error)) {
        std::cerr << error << '\n';
        return 1;
    }

    bool geriyeSardi = false;
    psforcer::DownloadSnapshot snapshot = waitFor(manager, &geriyeSardi);
    if (snapshot.state != psforcer::DownloadState::Completed ||
        psforcer::fileSize(destination) != payload.size() ||
        geriyeSardi) {
        std::cerr << "İndirme tamamlanamadı veya sayaç geriye gitti: "
                  << snapshot.error << '\n';
        return 1;
    }
    manager.reset();

    request.jobId = 2;
    request.expectedSize = payload.size() + 1;
    if (!manager.start(request, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    snapshot = waitFor(manager);
    if (snapshot.state != psforcer::DownloadState::Failed ||
        snapshot.error.find("Dosya boyutu") == std::string::npos) {
        std::cerr << "Yanlış dosya boyutu yakalanamadı\n";
        return 1;
    }

    manager.stopAndWait();
    std::remove(source.c_str());
    std::remove(destination.c_str());
    std::cout << "İndirme yöneticisi sınamaları geçti\n";
    return 0;
}
