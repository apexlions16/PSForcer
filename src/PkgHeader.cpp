#include "PkgHeader.h"

#include <cctype>

namespace psforcer {

namespace {
const size_t kRequiredHeaderSize = 0x438;
const uint32_t kPkgMagic = 0x7F434E54;

uint32_t readBe32(const std::vector<uint8_t>& data, size_t offset) {
    return (static_cast<uint32_t>(data[offset]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) |
           static_cast<uint32_t>(data[offset + 3]);
}

uint64_t readBe64(const std::vector<uint8_t>& data, size_t offset) {
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        value = (value << 8) | static_cast<uint64_t>(data[offset + i]);
    }
    return value;
}

bool validContentId(const std::string& value) {
    if (value.size() < 16 || value.size() > 36) return false;
    for (size_t i = 0; i < value.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (!std::isalnum(c) && c != '-' && c != '_') return false;
    }
    return true;
}
}

bool parsePkgHeader(const std::vector<uint8_t>& data,
                    PkgHeaderInfo& info,
                    std::string& error) {
    info = PkgHeaderInfo();
    if (data.size() < kRequiredHeaderSize) {
        error = "PKG başlığı eksik";
        return false;
    }
    if (readBe32(data, 0x000) != kPkgMagic) {
        error = "Uzak dosya geçerli bir PS4 PKG değil";
        return false;
    }

    std::string contentId;
    for (size_t i = 0; i < 0x24 && data[0x040 + i] != 0; ++i) {
        contentId.push_back(static_cast<char>(data[0x040 + i]));
    }
    if (!validContentId(contentId)) {
        error = "PKG Content ID okunamadı";
        return false;
    }

    const uint32_t contentType = readBe32(data, 0x074);
    switch (contentType) {
        case 0x01:
        case 0x1A:
            info.packageType = "PS4GD";
            break;
        case 0x02:
        case 0x1B:
            info.packageType = "PS4AC";
            break;
        case 0x03:
        case 0x1C:
            info.packageType = "PS4AL";
            break;
        case 0x04:
        case 0x05:
        case 0x1E:
            info.packageType = "PS4DP";
            break;
        default:
            error = "Desteklenmeyen PS4 PKG içerik türü";
            return false;
    }

    const uint32_t flags = readBe32(data, 0x078);
    info.patch = (flags & 0x00100000u) != 0 ||
                 (flags & 0x01000000u) != 0 ||
                 (flags & 0x20000000u) != 0 ||
                 (flags & 0x40000000u) != 0 ||
                 info.packageType == "PS4DP";
    info.contentId = contentId;
    info.packageSize = readBe64(data, 0x430);
    if (info.packageSize == 0) {
        error = "PKG boyutu başlıktan okunamadı";
        return false;
    }
    error.clear();
    return true;
}

}  // namespace psforcer
