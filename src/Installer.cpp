#include "Installer.h"
#include "FileUtil.h"
#include "HttpClient.h"
#include "PkgHeader.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

#if defined(PSFORCER_ORBIS)
#include <orbis/AppInstUtil.h>
#include <orbis/Bgft.h>
#include <orbis/UserService.h>
#endif

namespace psforcer {

namespace {
#if defined(PSFORCER_ORBIS)
std::string orbisError(const char* operation, int32_t result) {
    std::ostringstream message;
    message << operation << " başarısız oldu (0x"
            << std::uppercase << std::hex
            << static_cast<uint32_t>(result) << ')';
    return message.str();
}
#endif
}

InstallOutcome ManualInstaller::requestInstall(const CatalogItem& item,
                                                const PackageInfo& package,
                                                const std::string& packagePath) {
    const std::string markerPath = packagePath + ".hazir";
    std::ofstream marker(markerPath.c_str(), std::ios::out | std::ios::trunc);
    if (!marker) return InstallOutcome(InstallResult::Failed, "Hazır işareti yazılamadı");
    marker << "icerik=" << item.id << "\n";
    marker << "paket=" << package.id << "\n";
    marker << "tur=" << packageKindName(package.kind) << "\n";
    marker << "surum=" << package.version << "\n";
    marker << "yol=" << packagePath << "\n";
    marker.close();
    return InstallOutcome(
        InstallResult::ReadyForManualInstall,
        "Paket doğrulandı ve yetkili kurucu için hazırlandı");
}

InstallOutcome ManualInstaller::requestRemoteInstall(const CatalogItem& item,
                                                      const PackageInfo& package) {
    (void)item;
    (void)package;
    return InstallOutcome(InstallResult::Failed,
                          "Doğrudan PS4 indirmesi yalnızca PS4 derlemesinde kullanılabilir");
}

OrbisInstaller::OrbisInstaller()
    : bgftHeap_(NULL), bgftInitialized_(false), appInstInitialized_(false) {}

OrbisInstaller::~OrbisInstaller() {
#if defined(PSFORCER_ORBIS)
    if (bgftInitialized_) {
        sceBgftServiceIntTerm();
        bgftInitialized_ = false;
    }
    if (appInstInitialized_) {
        sceAppInstUtilTerminate();
        appInstInitialized_ = false;
    }
#endif
    std::free(bgftHeap_);
    bgftHeap_ = NULL;
}

InstallOutcome OrbisInstaller::requestInstall(const CatalogItem& item,
                                               const PackageInfo& package,
                                               const std::string& packagePath) {
#if !defined(PSFORCER_ORBIS)
    (void)item;
    (void)package;
    (void)packagePath;
    return InstallOutcome(InstallResult::Failed,
                          "Otomatik PKG kurulumu yalnızca PS4 derlemesinde kullanılabilir");
#else
    if (!fileExists(packagePath)) {
        return InstallOutcome(InstallResult::Failed,
                              "Kurulacak PKG dosyası bulunamadı");
    }

    const uint64_t actualSize = fileSize(packagePath);
    if (package.sizeBytes > 0 && actualSize != package.sizeBytes) {
        std::ostringstream message;
        message << "Kurulum durduruldu: PKG boyutu beklenen "
                << package.sizeBytes << " bayt yerine " << actualSize << " bayt";
        return InstallOutcome(InstallResult::Failed, message.str());
    }

    if (!appInstInitialized_) {
        const int32_t result = sceAppInstUtilInitialize();
        if (result != 0) {
            return InstallOutcome(InstallResult::Failed,
                                  orbisError("AppInstUtil başlatma", result));
        }
        appInstInitialized_ = true;
    }

    char titleId[16];
    std::memset(titleId, 0, sizeof(titleId));
    int32_t isApp = 0;
    int32_t result = sceAppInstUtilGetTitleIdFromPkg(packagePath.c_str(),
                                                     titleId, &isApp);
    if (result != 0) {
        return InstallOutcome(InstallResult::Failed,
                              orbisError("PKG kimliği okuma", result));
    }

    // Dosya artık ağ kaynağı değildir: doğrulanmış ve kapatılmış yerel PKG'yi
    // doğrudan AppInstUtil'a ver. BGFT download görevi kullanmak aynı dosyayı
    // yeniden bir indirme hedefi gibi işleyip boyutunun büyümesine yol açabiliyordu.
    result = sceAppInstUtilAppInstallPkg(packagePath.c_str(), NULL);
    if (result != 0) {
        return InstallOutcome(InstallResult::Failed,
                              orbisError("Yerel PKG kurulumunu başlatma", result));
    }

    std::ostringstream message;
    message << item.title << " tam boyutta indirildi; " << titleId
            << " yerel PKG kurulumu başlatıldı";
    return InstallOutcome(InstallResult::InstallStarted, message.str());
#endif
}

InstallOutcome OrbisInstaller::requestRemoteInstall(const CatalogItem& item,
                                                     const PackageInfo& package) {
#if !defined(PSFORCER_ORBIS)
    (void)item;
    (void)package;
    return InstallOutcome(InstallResult::Failed,
                          "Doğrudan PS4 indirmesi yalnızca PS4 derlemesinde kullanılabilir");
#else
    if (package.url.empty() || package.sizeBytes == 0) {
        return InstallOutcome(InstallResult::Failed,
                              "Uzak PKG adresi veya kesin boyutu eksik");
    }
    if (package.sizeBytes > std::numeric_limits<uint32_t>::max()) {
        return InstallOutcome(InstallResult::Failed,
                              "PKG boyutu PS4 BGFT sınırını aşıyor");
    }

    HttpClient resolver;
    std::string resolvedUrl;
    std::vector<uint8_t> headerData;
    std::string error;
    if (!resolver.resolvePackageHeader(package.url, package.sizeBytes,
                                       resolvedUrl, headerData, error)) {
        return InstallOutcome(InstallResult::Failed, error);
    }

    PkgHeaderInfo header;
    if (!parsePkgHeader(headerData, header, error)) {
        return InstallOutcome(InstallResult::Failed, error);
    }
    if (header.packageSize != package.sizeBytes) {
        std::ostringstream message;
        message << "PKG başlığındaki boyut katalogla uyuşmuyor. Beklenen "
                << package.sizeBytes << " bayt, başlıkta " << header.packageSize;
        return InstallOutcome(InstallResult::Failed, message.str());
    }
    if (resolvedUrl.size() >= 0x800) {
        return InstallOutcome(InstallResult::Failed,
                              "İmzalı PKG adresi PS4 BGFT sınırını aşıyor");
    }

    if (!bgftInitialized_) {
        const size_t heapSize = 1024 * 1024;
        bgftHeap_ = std::malloc(heapSize);
        if (!bgftHeap_) {
            return InstallOutcome(InstallResult::Failed,
                                  "PS4 indirme hizmeti için bellek ayrılamadı");
        }
        std::memset(bgftHeap_, 0, heapSize);
        OrbisBgftInitParams initParams;
        std::memset(&initParams, 0, sizeof(initParams));
        initParams.heap = bgftHeap_;
        initParams.heapSize = heapSize;
        const int32_t initResult = sceBgftServiceIntInit(&initParams);
        if (initResult != 0) {
            std::free(bgftHeap_);
            bgftHeap_ = NULL;
            return InstallOutcome(InstallResult::Failed,
                                  orbisError("PS4 indirme hizmetini başlatma", initResult));
        }
        bgftInitialized_ = true;
    }

    int32_t userId = -1;
    int32_t result = sceUserServiceGetForegroundUser(&userId);
    if (result != 0) {
        return InstallOutcome(InstallResult::Failed,
                              orbisError("Aktif PS4 kullanıcısını alma", result));
    }
    if (userId < 0) {
        return InstallOutcome(InstallResult::Failed,
                              "Aktif PS4 kullanıcısı bulunamadı");
    }

    const std::string displayName = item.title + " - " + package.label;
    OrbisBgftDownloadParam params;
    std::memset(&params, 0, sizeof(params));
    params.userId = userId;
    params.entitlementType = 5;
    params.id = header.contentId.c_str();
    params.contentUrl = resolvedUrl.c_str();
    params.contentName = displayName.c_str();
    params.iconPath = "";
    params.playgoScenarioId = "0";
    params.option = ORBIS_BGFT_TASK_OPT_DISABLE_CDN_QUERY_PARAM;
    params.packageType = header.packageType.c_str();
    params.packageSubType = "";
    params.packageSize = static_cast<uint32_t>(header.packageSize);

    OrbisBgftTaskId taskId = -1;
    result = header.patch
        ? sceBgftServiceIntDebugDownloadRegisterPkg(&params, &taskId)
        : sceBgftServiceIntDownloadRegisterTask(&params, &taskId);
    if (result != 0) {
        return InstallOutcome(InstallResult::Failed,
                              orbisError("PS4 indirme görevini oluşturma", result));
    }

    result = sceBgftServiceDownloadStartTask(taskId);
    if (result != 0) {
        sceBgftServiceIntDownloadUnregisterTask(taskId);
        return InstallOutcome(InstallResult::Failed,
                              orbisError("PS4 indirme görevini başlatma", result));
    }

    std::ostringstream message;
    message << displayName
            << " PS4 indirme ve kurulum kuyruğuna eklendi";
    return InstallOutcome(InstallResult::InstallStarted, message.str());
#endif
}

}  // namespace psforcer
