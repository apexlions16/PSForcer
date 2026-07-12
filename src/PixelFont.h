#pragma once

#include <algorithm>
#include <stdint.h>
#include <string>

namespace psforcer {

static const uint64_t kPixelHarfler[95] = {
    0x000000000ULL,0x39CE639C0ULL,0x7BDE00000ULL,0x01FFFFFC0ULL,0x3BDEF79CCULL,
    0x639FFFDE0ULL,0x3BDFFFFE0ULL,0x318C00000ULL,0x39CC631C6ULL,0x31CE739CCULL,
    0x33DEF3000ULL,0x019FFB180ULL,0x000E73000ULL,0x01CE00000ULL,0x000E73800ULL,
    0x18CE73318ULL,0x3BDFFFFC0ULL,0x7BCE73BE0ULL,0x7BCE77BC0ULL,0x3BCE7FFC0ULL,
    0x39DEFFCC0ULL,0x7BDE37BC0ULL,0x3BDEFFFC0ULL,0x7BCE73980ULL,0x3BDEFFFC0ULL,
    0x3BDFFFFC0ULL,0x01CE739C0ULL,0x01CE73980ULL,0x01FEFBC00ULL,0x03FFF8000ULL,
    0x03DFFF800ULL,0x3BCE63180ULL,0x3BFFFFFEFULL,0x39DEF7FE0ULL,0x7BFFFFFE0ULL,
    0x3BD8C73C0ULL,0x7BFFFFFC0ULL,0x7FFEF63E0ULL,0x7FFEF6300ULL,0x3BD8FFFE0ULL,
    0x7FFFFFFE0ULL,0x7BCE73BC0ULL,0x39C637BC0ULL,0x7FFEF7FE0ULL,0x6318C63E0ULL,
    0x7FFFFFFE0ULL,0x7FFFFFFE0ULL,0x3BFFFFFC0ULL,0x7BFFF6300ULL,0x3BFFFFFC6ULL,
    0x7BFFF7FE0ULL,0x3BDE7FFC0ULL,0x7FEE739C0ULL,0x7FFFFFFC0ULL,0x7FFEF79C0ULL,
    0x6FFFFFFE0ULL,0x7FEEF7BE0ULL,0x7FFE739C0ULL,0x7FEEF73E0ULL,0x398C631CEULL,
    0x630C738C6ULL,0x39CE739CEULL,0x3BFF00000ULL,0x0000FFC00ULL,0x218000000ULL,
    0x03DFFFFE0ULL,0x631EFFFFEULL,0x03DEF79C0ULL,0x18DEF7BDEULL,0x03DFFFDC0ULL,
    0x39DE6318CULL,0x3FFFFFFCEULL,0x631EF7BDEULL,0x39DE73BFFULL,0x3BCE739DEULL,
    0x631FF7BFFULL,0x738C631EFULL,0x03FFFFFE0ULL,0x03DEF7BC0ULL,0x03DFFFDC0ULL,
    0x7BFFFFB18ULL,0x7BDEF78C6ULL,0x01EF63180ULL,0x03DEF79C0ULL,0x33DE631C0ULL,
    0x03DEF7BC0ULL,0x03FEF39C0ULL,0x03FFFFFC0ULL,0x03DEF7FE0ULL,0x7FFE73BDCULL,
    0x03CEF7BC0ULL,0x39DEF39CEULL,0x318C6318CULL,0x71CE73B9CULL,0x001F70000ULL
};

struct YaziOlculeri {
    int yatay;
    int dikey;
    int ilerleme;
};

inline YaziOlculeri yaziOlculeri(int yukseklik) {
    YaziOlculeri sonuc;
    sonuc.dikey = std::max(1, yukseklik / 7);
    sonuc.yatay = std::max(1, (sonuc.dikey + 1) / 2);
    sonuc.ilerleme = 5 * sonuc.yatay + std::max(1, sonuc.yatay);
    return sonuc;
}

inline std::string turkceyiPikselYaziyaCevir(const std::string& deger) {
    std::string sonuc;
    for (size_t i = 0; i < deger.size(); ++i) {
        const unsigned char a = static_cast<unsigned char>(deger[i]);
        if (a >= 32 && a <= 126) { sonuc.push_back(static_cast<char>(a)); continue; }
        if (i + 1 >= deger.size()) continue;
        const unsigned char b = static_cast<unsigned char>(deger[i + 1]);
        char harf = 0;
        if (a == 0xC3 && (b == 0x87 || b == 0xA7)) harf = b == 0x87 ? 'C' : 'c';
        else if (a == 0xC4 && (b == 0x9E || b == 0x9F)) harf = b == 0x9E ? 'G' : 'g';
        else if (a == 0xC4 && (b == 0xB0 || b == 0xB1)) harf = b == 0xB0 ? 'I' : 'i';
        else if (a == 0xC3 && (b == 0x96 || b == 0xB6)) harf = b == 0x96 ? 'O' : 'o';
        else if (a == 0xC5 && (b == 0x9E || b == 0x9F)) harf = b == 0x9E ? 'S' : 's';
        else if (a == 0xC3 && (b == 0x9C || b == 0xBC)) harf = b == 0x9C ? 'U' : 'u';
        if (harf) { sonuc.push_back(harf); ++i; }
    }
    return sonuc;
}

}  // namespace psforcer
