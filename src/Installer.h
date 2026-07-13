#pragma once

#include "AppTypes.h"

#include <string>

namespace psforcer {

enum class InstallResult {
    Installed,
    InstallStarted,
    ReadyForManualInstall,
    Failed
};

struct InstallOutcome {
    InstallResult result;
    std::string message;
    InstallOutcome(InstallResult value = InstallResult::Failed, const std::string& text = std::string())
        : result(value), message(text) {}
};

class Installer {
public:
    virtual ~Installer() {}
    virtual InstallOutcome requestInstall(const CatalogItem& item,
                                          const PackageInfo& package,
                                          const std::string& packagePath) = 0;
};

class ManualInstaller : public Installer {
public:
    virtual InstallOutcome requestInstall(const CatalogItem& item,
                                          const PackageInfo& package,
                                          const std::string& packagePath);
};

class OrbisInstaller : public Installer {
public:
    OrbisInstaller();
    virtual ~OrbisInstaller();
    virtual InstallOutcome requestInstall(const CatalogItem& item,
                                          const PackageInfo& package,
                                          const std::string& packagePath);
private:
    void* bgftHeap_;
    bool bgftInitialized_;
    bool appInstInitialized_;
};

}  // namespace psforcer
