#include "Installer.h"
#include "FileUtil.h"

#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

#if defined(PSFORCER_ORBIS)
#include <orbis/AppInstUtil.h>
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
    : appInstInitialized_(false) {}

OrbisInstaller::~OrbisInstaller() {
#if defined(PSFORCER_ORBIS)
    if (appInstInitialized_) {
        sceAppInstUtilTerminate();
        appInstInitialized_ = false;
    }
#endif
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

}  // namespace psforcer
