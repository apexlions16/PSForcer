#include "Installer.h"

#include <fstream>

namespace psforcer {

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

}  // namespace psforcer
