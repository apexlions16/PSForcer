#include "Sha256.h"

#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace psforcer {

namespace {
const uint32_t kRoundConstants[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
inline uint32_t rotateRight(uint32_t value, uint32_t count) { return (value >> count) | (value << (32 - count)); }
inline uint32_t choose(uint32_t e, uint32_t f, uint32_t g) { return (e & f) ^ (~e & g); }
inline uint32_t majority(uint32_t a, uint32_t b, uint32_t c) { return (a & b) ^ (a & c) ^ (b & c); }
inline uint32_t bigSigma0(uint32_t x) { return rotateRight(x, 2) ^ rotateRight(x, 13) ^ rotateRight(x, 22); }
inline uint32_t bigSigma1(uint32_t x) { return rotateRight(x, 6) ^ rotateRight(x, 11) ^ rotateRight(x, 25); }
inline uint32_t smallSigma0(uint32_t x) { return rotateRight(x, 7) ^ rotateRight(x, 18) ^ (x >> 3); }
inline uint32_t smallSigma1(uint32_t x) { return rotateRight(x, 17) ^ rotateRight(x, 19) ^ (x >> 10); }
}

Sha256::Sha256() : bitLength_(0), bufferLength_(0), finalized_(false) {
    state_[0] = 0x6a09e667; state_[1] = 0xbb67ae85; state_[2] = 0x3c6ef372; state_[3] = 0xa54ff53a;
    state_[4] = 0x510e527f; state_[5] = 0x9b05688c; state_[6] = 0x1f83d9ab; state_[7] = 0x5be0cd19;
    std::memset(buffer_, 0, sizeof(buffer_));
}

void Sha256::update(const uint8_t* data, size_t length) {
    if (finalized_) return;
    for (size_t i = 0; i < length; ++i) {
        buffer_[bufferLength_++] = data[i];
        if (bufferLength_ == 64) { transform(buffer_); bitLength_ += 512; bufferLength_ = 0; }
    }
}

void Sha256::update(const std::string& data) { update(reinterpret_cast<const uint8_t*>(data.data()), data.size()); }

std::string Sha256::finalHex() {
    if (!finalized_) {
        size_t i = bufferLength_;
        buffer_[i++] = 0x80;
        if (i > 56) { while (i < 64) buffer_[i++] = 0; transform(buffer_); i = 0; }
        while (i < 56) buffer_[i++] = 0;
        bitLength_ += static_cast<uint64_t>(bufferLength_) * 8;
        for (int shift = 56; shift >= 0; shift -= 8) buffer_[i++] = static_cast<uint8_t>((bitLength_ >> shift) & 0xFF);
        transform(buffer_);
        finalized_ = true;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (int i = 0; i < 8; ++i) output << std::setw(8) << state_[i];
    return output.str();
}

bool Sha256::fileHex(const std::string& path, std::string& digest, std::string& error) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) { error = "SHA-256 için dosya açılamadı: " + path; return false; }
    Sha256 sha;
    uint8_t buffer[64 * 1024];
    while (true) {
        const size_t read = std::fread(buffer, 1, sizeof(buffer), file);
        if (read > 0) sha.update(buffer, read);
        if (read < sizeof(buffer)) {
            if (std::ferror(file)) {
                std::fclose(file);
                error = "SHA-256 hesaplanırken dosya okuma hatası oluştu";
                return false;
            }
            break;
        }
    }
    std::fclose(file);
    digest = sha.finalHex();
    return true;
}

void Sha256::transform(const uint8_t block[64]) {
    uint32_t words[64];
    for (int i = 0; i < 16; ++i) {
        words[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
                   (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
                   static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) words[i] = smallSigma1(words[i - 2]) + words[i - 7] + smallSigma0(words[i - 15]) + words[i - 16];
    uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
    for (int i = 0; i < 64; ++i) {
        const uint32_t temp1 = h + bigSigma1(e) + choose(e, f, g) + kRoundConstants[i] + words[i];
        const uint32_t temp2 = bigSigma0(a) + majority(a, b, c);
        h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
    }
    state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
}

}  // namespace psforcer
