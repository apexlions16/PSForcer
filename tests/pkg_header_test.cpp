#include "PkgHeader.h"

#include <iostream>
#include <string>
#include <vector>

namespace {
void writeBe32(std::vector<uint8_t>& data, size_t offset, uint32_t value) {
    data[offset] = static_cast<uint8_t>(value >> 24);
    data[offset + 1] = static_cast<uint8_t>(value >> 16);
    data[offset + 2] = static_cast<uint8_t>(value >> 8);
    data[offset + 3] = static_cast<uint8_t>(value);
}

void writeBe64(std::vector<uint8_t>& data, size_t offset, uint64_t value) {
    for (int i = 7; i >= 0; --i) {
        data[offset + static_cast<size_t>(i)] = static_cast<uint8_t>(value);
        value >>= 8;
    }
}
}

int main() {
    const std::string contentId = "UP0000-CUSA01127_00-PTPACKAGE0000001";
    const uint64_t packageSize = 1478164480ULL;
    std::vector<uint8_t> header(0x438, 0);
    writeBe32(header, 0x000, 0x7F434E54);
    for (size_t i = 0; i < contentId.size(); ++i) header[0x040 + i] = contentId[i];
    writeBe32(header, 0x074, 0x1A);
    writeBe32(header, 0x078, 0);
    writeBe64(header, 0x430, packageSize);

    psforcer::PkgHeaderInfo info;
    std::string error;
    if (!psforcer::parsePkgHeader(header, info, error) ||
        info.contentId != contentId ||
        info.packageType != "PS4GD" ||
        info.packageSize != packageSize || info.patch) {
        std::cerr << "PKG başlığı çözümlenemedi: " << error << '\n';
        return 1;
    }

    writeBe32(header, 0x078, 0x00100000);
    if (!psforcer::parsePkgHeader(header, info, error) || !info.patch) {
        std::cerr << "PKG yama bayrağı çözümlenemedi: " << error << '\n';
        return 1;
    }

    std::cout << "PKG başlığı sınamaları geçti\n";
    return 0;
}
