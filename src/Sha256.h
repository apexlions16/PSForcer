#pragma once

#include <stdint.h>
#include <string>

namespace psforcer {

class Sha256 {
public:
    Sha256();
    void update(const uint8_t* data, size_t length);
    void update(const std::string& data);
    std::string finalHex();
    static bool fileHex(const std::string& path, std::string& digest, std::string& error);
private:
    void transform(const uint8_t block[64]);
    uint32_t state_[8];
    uint64_t bitLength_;
    uint8_t buffer_[64];
    size_t bufferLength_;
    bool finalized_;
};

}  // namespace psforcer
