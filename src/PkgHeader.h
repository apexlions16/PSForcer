#pragma once

#include <stdint.h>
#include <string>
#include <vector>

namespace psforcer {

struct PkgHeaderInfo {
    std::string contentId;
    std::string packageType;
    uint64_t packageSize;
    bool patch;
    PkgHeaderInfo() : packageSize(0), patch(false) {}
};

bool parsePkgHeader(const std::vector<uint8_t>& data,
                    PkgHeaderInfo& info,
                    std::string& error);

}  // namespace psforcer
