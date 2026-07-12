#pragma once

#include "AppTypes.h"

#include <string>

namespace psforcer {

class CatalogLoader {
public:
    static bool loadFile(const std::string& path, CatalogData& catalog, std::string& error);
    static bool parse(const std::string& json, CatalogData& catalog, std::string& error);
    static bool itemContainsKind(const CatalogItem& item, PackageKind kind);
    static PackageKind parseKind(const std::string& value);
    static std::string formatBytes(uint64_t bytes);
};

}  // namespace psforcer
