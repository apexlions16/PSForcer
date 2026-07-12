#pragma once

#include <stdint.h>
#include <string>
#include <vector>

namespace psforcer {

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;

    Color(uint8_t red = 255, uint8_t green = 255, uint8_t blue = 255, uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}
};

enum class PackageKind {
    Game,
    Update,
    Dlc,
    Extra,
    Unknown
};

inline const char* packageKindName(PackageKind kind) {
    switch (kind) {
        case PackageKind::Game: return "OYUN";
        case PackageKind::Update: return "GÜNCELLEME";
        case PackageKind::Dlc: return "EK PAKET";
        case PackageKind::Extra: return "EKSTRA";
        default: return "DİĞER";
    }
}

struct MediaInfo {
    std::string cover;
    std::string hero;
    std::string trailer;
    std::vector<std::string> screenshots;
};

struct PackageInfo {
    std::string id;
    PackageKind kind;
    std::string label;
    std::string version;
    uint64_t sizeBytes;
    std::string url;
    std::string sha256;
    std::string minFirmware;
    bool deleteAfterInstall;

    PackageInfo()
        : kind(PackageKind::Unknown), sizeBytes(0), deleteAfterInstall(false) {}
};

struct CatalogItem {
    std::string id;
    std::string title;
    std::string developer;
    std::string genre;
    std::string description;
    int releaseYear;
    Color accent;
    MediaInfo media;
    std::vector<PackageInfo> packages;

    CatalogItem() : releaseYear(0), accent(108, 99, 255, 255) {}
};

struct CatalogData {
    int schemaVersion;
    std::string catalogTitle;
    std::string updatedAt;
    std::vector<CatalogItem> items;

    CatalogData() : schemaVersion(0) {}
};

}  // namespace psforcer
