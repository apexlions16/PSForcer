#include "Installer.h"
#include "FileUtil.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

#if defined(PSFORCER_ORBIS)
#include <orbis/AppInstUtil.h>
#include <orbis/Bgft.h>
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

    if (!appInstInitialized_) {
        const int32_t result = sceAppInstUtilInitialize();
        if (result != 0) {
            return InstallOutcome(InstallResult::Failed,
                                  orbisError("AppInstUtil başlatma", result));
        }
        appInstInitialized_ = true;
    }

    if (!bgftInitialized_) {
        const size_t heapSize = 1024 * 1024;
        bgftHeap_ = std::malloc(heapSize);
        if (!bgftHeap_) {
            return InstallOutcome(InstallResult::Failed,
                                  "BGFT kurulumu için bellek ayrılamadı");
        }
        std::memset(bgftHeap_, 0, heapSize);

        OrbisBgftInitParams params;
        std::memset(&params, 0, sizeof(params));
        params.heap = bgftHeap_;
        params.heapSize = heapSize;
        const int32_t result = sceBgftServiceIntInit(&params);
        if (result != 0) {
            std::free(bgftHeap_);
            bgftHeap_ = NULL;
            return InstallOutcome(InstallResult::Failed,
                                  orbisError("BGFT başlatma", result));
        }
        bgftInitialized_ = true;
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

    const std::string contentName = item.title + " - " + package.label;
    OrbisBgftDownloadParamEx params;
    std::memset(&params, 0, sizeof(params));
    params.params.entitlementType = 5;
    params.params.id = "";
    params.params.contentUrl = packagePath.c_str();
    params.params.contentName = contentName.c_str();
    params.params.iconPath = "/app0/sce_sys/icon0.png";
    params.params.option = ORBIS_BGFT_TASK_OPT_INVISIBLE;
    params.params.playgoScenarioId = "0";
    params.slot = 0;

    OrbisBgftTaskId taskId = -1;
    result = sceBgftServiceIntDownloadRegisterTaskByStorageEx(&params, &taskId);
    if (result != 0) {
        return InstallOutcome(InstallResult::Failed,
                              orbisError("PKG kurulum görevi oluşturma", result));
    }

    result = sceBgftServiceDownloadStartTask(taskId);
    if (result != 0) {
        sceBgftServiceIntDownloadUnregisterTask(taskId);
        return InstallOutcome(InstallResult::Failed,
                              orbisError("PKG kurulumunu başlatma", result));
    }

    std::ostringstream message;
    message << "İndirme tamamlandı; " << titleId
            << " kurulumu PS4 arka plan hizmetine teslim edildi";
    return InstallOutcome(InstallResult::InstallStarted, message.str());
#endif
}

}  // namespace psforcer
