#include "App.h"
#include "FileUtil.h"

#include <cstdio>

namespace psforcer {

namespace {
bool sonDurum(DownloadState state) {
    return state == DownloadState::Completed || state == DownloadState::Failed || state == DownloadState::Cancelled;
}
}

void App::startPackageDownload() {
    if (downloads_.busy() || visible_.empty()) {
        if (downloads_.busy()) setToast("Bir paket indirmesi zaten çalışıyor");
        return;
    }

    const size_t itemIndex = visible_[selectedVisible_];
    const CatalogItem& item = catalog_.items[itemIndex];
    if (item.packages.empty() || selectedPackage_ >= item.packages.size()) return;
    const PackageInfo& package = item.packages[selectedPackage_];
    if (package.url.empty()) {
        setToast("Bu paket için indirme bağlantısı eklenmedi");
        return;
    }
    if (package.sizeBytes == 0) {
        setToast("Katalogdaki kesin paket boyutu eksik; güvenli indirme başlatılmadı", 7000);
        return;
    }

    // Kullanıcı dosyayı FTP ile silmiş olsa bile indirmeden önce güvenli şablonu
    // geri getir. Gerçek token hiçbir zaman paket veya GitHub içine yazılmaz.
    if (!ensureHuggingFaceTokenFile()) {
        setToast("/data/psforcer/hf_token.txt yeniden oluşturulamadı", 7000);
        return;
    }

#if defined(PSFORCER_ORBIS)
    // Büyük PKG'yi uygulama belleği veya /data üzerinden kopyalama. Yalnızca
    // küçük PKG başlığı ve imzalı son HTTPS adresi çözülür; asıl indirme ve
    // kurulum PS4'ün yerel BGFT hizmeti tarafından yürütülür.
    const InstallOutcome remoteOutcome = installer_->requestRemoteInstall(item, package);
    if (remoteOutcome.result == InstallResult::InstallStarted) {
        status_ = "PS4 indiriyor ve kuracak";
    } else {
        status_ = "PS4 indirme görevi başlatılamadı";
    }
    setToast(remoteOutcome.message, 7500);
    return;
#endif

    const std::string baseName = sanitizeFileName(item.id + "-" + package.id + "-" + package.version + ".pkg");
    pendingFinalPath_ = runtimeRoot() + "/indirmeler/" + baseName;
    const std::string partialPath = pendingFinalPath_ + ".parca";

    // Paket indirmeleri hiçbir zaman eski bir .parca dosyasına eklenmez. Önceki
    // başarısız denemeden kalan veri hedef boyutu aşabilir veya başka bir HTTP
    // yanıtına ait olabilir; her kullanıcı başlatmasında dosya sıfırdan kurulur.
    std::remove(partialPath.c_str());
    std::remove((pendingFinalPath_ + ".hazir").c_str());
    if (fileExists(partialPath)) {
        setToast("Eski .parca dosyası silinemedi; FTP bağlantısını kapatıp tekrar deneyin", 7500);
        return;
    }

    DownloadRequest request;
    request.jobId = nextJobId_++;
    request.id = package.id;
    request.label = item.title + " - " + package.label;
    request.url = package.url;
    request.destination = partialPath;
    request.sha256 = package.sha256;
    request.expectedSize = package.sizeBytes;
    request.resume = false;

    std::string error;
    if (!downloads_.start(request, error)) {
        setToast(error, 6500);
        status_ = "İndirme başlatılamadı";
        return;
    }

    pendingItemIndex_ = itemIndex;
    pendingPackageIndex_ = selectedPackage_;
    status_ = "Temiz indirme başlatıldı";
}

void App::refreshCatalog(bool silent) {
    if (catalogDownloads_.busy()) {
        if (!silent) setToast("Katalog zaten yenileniyor");
        return;
    }

    std::string url = readFirstLine(runtimeRoot() + "/katalog_adresi.txt");
    if (url.empty()) url = readFirstLine(bundledPath("assets/katalog_adresi.txt"));
    if (url.empty()) {
        if (!silent) setToast("/data/psforcer/katalog_adresi.txt dosyasına katalog bağlantısını yazın", 6500);
        return;
    }

    DownloadRequest request;
    request.jobId = nextJobId_++;
    request.id = "katalog-yenileme";
    request.label = "Katalog yenileniyor";
    request.url = url;
    request.destination = runtimeRoot() + "/katalog.json.parca";
    request.resume = false;

    std::string error;
    if (!catalogDownloads_.start(request, error)) {
        if (!silent) setToast(error, 6500);
        return;
    }

    catalogRefreshSilent_ = silent;
    catalogFinalPath_ = runtimeRoot() + "/katalog.json";
    status_ = "Katalog indiriliyor";
}

void App::processCatalogCompletion() {
    const DownloadSnapshot snapshot = catalogDownloads_.snapshot();
    if (!sonDurum(snapshot.state) || snapshot.jobId == 0 || snapshot.jobId == lastHandledCatalogJobId_) return;
    lastHandledCatalogJobId_ = snapshot.jobId;

    if (snapshot.state == DownloadState::Failed || snapshot.state == DownloadState::Cancelled) {
        status_ = "Yerleşik katalog kullanılıyor";
        if (!catalogRefreshSilent_) {
            setToast(snapshot.error.empty() ? "Katalog yenilenemedi" : snapshot.error, 6500);
        }
        catalogDownloads_.reset();
        return;
    }

    std::remove(catalogFinalPath_.c_str());
    if (std::rename(snapshot.destination.c_str(), catalogFinalPath_.c_str()) != 0) {
        if (!catalogRefreshSilent_) setToast("Katalog dosyası etkinleştirilemedi", 6500);
        catalogDownloads_.reset();
        return;
    }

    std::string error;
    if (loadCatalog(error)) {
        selectedVisible_ = 0;
        selectedPackage_ = 0;
        rebuildVisible();
        status_ = "Katalog güncel";
        if (!catalogRefreshSilent_) setToast("Katalog yenilendi");
    } else {
        status_ = "Yeni katalog geçersiz";
        if (!catalogRefreshSilent_) setToast("Yeni katalog geçersiz: " + error, 7000);
    }

    catalogDownloads_.reset();
}

void App::processDownloadCompletion() {
    const DownloadSnapshot snapshot = downloads_.snapshot();
    if (!sonDurum(snapshot.state) || snapshot.jobId == 0 || snapshot.jobId == lastHandledJobId_) return;
    lastHandledJobId_ = snapshot.jobId;

    if (snapshot.state == DownloadState::Failed || snapshot.state == DownloadState::Cancelled) {
        status_ = snapshot.state == DownloadState::Cancelled ? "İndirme iptal edildi" : "İndirme başarısız";
        setToast(snapshot.error.empty() ? status_ : snapshot.error, 7000);
        downloads_.reset();
        return;
    }

    std::remove(pendingFinalPath_.c_str());
    if (std::rename(snapshot.destination.c_str(), pendingFinalPath_.c_str()) != 0) {
        setToast("Doğrulanan paket yeniden adlandırılamadı", 6500);
        downloads_.reset();
        return;
    }

    if (pendingItemIndex_ >= catalog_.items.size() ||
        pendingPackageIndex_ >= catalog_.items[pendingItemIndex_].packages.size()) {
        setToast("İndirme katalog kaydıyla eşleştirilemedi", 6500);
        downloads_.reset();
        return;
    }

    const CatalogItem& item = catalog_.items[pendingItemIndex_];
    const PackageInfo& package = item.packages[pendingPackageIndex_];
    const InstallOutcome outcome = installer_->requestInstall(item, package, pendingFinalPath_);
    if (outcome.result == InstallResult::Installed && package.deleteAfterInstall) {
        std::remove(pendingFinalPath_.c_str());
        status_ = "Kuruldu ve paket silindi";
    } else if (outcome.result == InstallResult::InstallStarted) {
        status_ = "Tam PKG yerel kurucuya teslim edildi";
    } else if (outcome.result == InstallResult::ReadyForManualInstall) {
        status_ = "Paket kurulum için hazır";
    } else {
        status_ = "Kurulum teslimi başarısız";
    }
    setToast(outcome.message, 6500);
    downloads_.reset();
}

}  // namespace psforcer
