#include "Installer.h"

#include <fstream>

namespace psforcer {

InstallOutcome ManualInstaller::requestInstall(const CatalogItem& item,
                                                const PackageInfo& package,
                                                const std::string& packagePath) {
    const std::string markerPath = packagePath + ".ready";
    std::ofstream marker(markerPath.c_str(), std::ios::out | std::ios::trunc);
    if (!marker) return InstallOutcome(InstallResult::Failed, "Ready marker could not be written");
    marker << "item=" << item.id << "\n";
    marker << "package=" << package.id << "\n";
    marker << "kind=" << packageKindName(package.kind) << "\n";
    marker << "version=" << package.version << "\n";
    marker << "path=" << packagePath << "\n";
    marker.close();
    return InstallOutcome(
        InstallResult::ReadyForManualInstall,
        "Package verified and ready for the authorized installer");
}

}  // namespace psforcer
