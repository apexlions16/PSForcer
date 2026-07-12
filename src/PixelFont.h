#pragma once

#include <algorithm>
#include <cctype>
#include <stdint.h>
#include <string>

namespace psforcer {

struct PixelHarf {
    char karakter;
    uint8_t satirlar[7];
};

// Temiz ve sabit 5x7 yazı takımı. Önceki sıkıştırılmış bit tablosu bazı
// harfleri birbirine karıştırdığı için her satır açık biçimde tanımlanır.
static const PixelHarf kPixelHarfler[] = {
    {' ', {0, 0, 0, 0, 0, 0, 0}},
    {'!', {4, 4, 4, 4, 4, 0, 4}},
    {'"', {10, 10, 10, 0, 0, 0, 0}},
    {'#', {10, 31, 10, 10, 31, 10, 0}},
    {'$', {4, 15, 20, 14, 5, 30, 4}},
    {'%', {25, 25, 2, 4, 8, 19, 19}},
    {'&', {12, 18, 20, 8, 21, 18, 13}},
    {'\'', {4, 4, 8, 0, 0, 0, 0}},
    {'(', {2, 4, 8, 8, 8, 4, 2}},
    {')', {8, 4, 2, 2, 2, 4, 8}},
    {'*', {0, 21, 14, 31, 14, 21, 0}},
    {'+', {0, 4, 4, 31, 4, 4, 0}},
    {',', {0, 0, 0, 0, 4, 4, 8}},
    {'-', {0, 0, 0, 31, 0, 0, 0}},
    {'.', {0, 0, 0, 0, 0, 12, 12}},
    {'/', {1, 2, 4, 8, 16, 0, 0}},
    {'0', {14, 17, 19, 21, 25, 17, 14}},
    {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}},
    {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}},
    {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}},
    {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}},
    {'9', {14, 17, 17, 15, 1, 1, 14}},
    {':', {0, 12, 12, 0, 12, 12, 0}},
    {';', {0, 12, 12, 0, 4, 4, 8}},
    {'<', {2, 4, 8, 16, 8, 4, 2}},
    {'=', {0, 31, 0, 31, 0, 0, 0}},
    {'>', {8, 4, 2, 1, 2, 4, 8}},
    {'?', {14, 17, 1, 2, 4, 0, 4}},
    {'@', {14, 17, 23, 21, 23, 16, 14}},
    {'A', {14, 17, 17, 31, 17, 17, 17}},
    {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}},
    {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}},
    {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 15}},
    {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {31, 4, 4, 4, 4, 4, 31}},
    {'J', {7, 2, 2, 2, 18, 18, 12}},
    {'K', {17, 18, 20, 24, 20, 18, 17}},
    {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}},
    {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}},
    {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}},
    {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}},
    {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}},
    {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 21, 10}},
    {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}},
    {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {'[', {14, 8, 8, 8, 8, 8, 14}},
    {'\\', {16, 8, 4, 2, 1, 0, 0}},
    {']', {14, 2, 2, 2, 2, 2, 14}},
    {'^', {4, 10, 17, 0, 0, 0, 0}},
    {'_', {0, 0, 0, 0, 0, 0, 31}},
    {'`', {8, 4, 2, 0, 0, 0, 0}},
    {'{', {2, 4, 4, 8, 4, 4, 2}},
    {'|', {4, 4, 4, 4, 4, 4, 4}},
    {'}', {8, 4, 4, 2, 4, 4, 8}},
    {'~', {0, 0, 9, 22, 0, 0, 0}}
};

struct YaziOlculeri {
    int yatay;
    int dikey;
    int ilerleme;
};

inline YaziOlculeri yaziOlculeri(int yukseklik) {
    YaziOlculeri sonuc;
    sonuc.dikey = std::max(1, yukseklik / 7);
    sonuc.yatay = std::max(1, (sonuc.dikey * 2 + 2) / 3);
    sonuc.ilerleme = 5 * sonuc.yatay + sonuc.yatay;
    return sonuc;
}

inline char buyukAscii(char karakter) {
    if (karakter >= 'a' && karakter <= 'z') return static_cast<char>(karakter - 'a' + 'A');
    return karakter;
}

inline const uint8_t* pixelHarf(char karakter) {
    karakter = buyukAscii(karakter);
    const size_t sayi = sizeof(kPixelHarfler) / sizeof(kPixelHarfler[0]);
    for (size_t i = 0; i < sayi; ++i) {
        if (kPixelHarfler[i].karakter == karakter) return kPixelHarfler[i].satirlar;
    }
    return pixelHarf('?');
}

inline std::string turkceyiPikselYaziyaCevir(const std::string& deger) {
    std::string sonuc;
    for (size_t i = 0; i < deger.size(); ++i) {
        const unsigned char a = static_cast<unsigned char>(deger[i]);
        if (a >= 32 && a <= 126) {
            sonuc.push_back(buyukAscii(static_cast<char>(a)));
            continue;
        }
        if (i + 1 >= deger.size()) continue;
        const unsigned char b = static_cast<unsigned char>(deger[i + 1]);
        char harf = 0;
        if (a == 0xC3 && (b == 0x87 || b == 0xA7)) harf = 'C';
        else if (a == 0xC4 && (b == 0x9E || b == 0x9F)) harf = 'G';
        else if (a == 0xC4 && (b == 0xB0 || b == 0xB1)) harf = 'I';
        else if (a == 0xC3 && (b == 0x96 || b == 0xB6)) harf = 'O';
        else if (a == 0xC5 && (b == 0x9E || b == 0x9F)) harf = 'S';
        else if (a == 0xC3 && (b == 0x9C || b == 0xBC)) harf = 'U';
        if (harf) {
            sonuc.push_back(harf);
            ++i;
        }
    }
    return sonuc;
}

}  // namespace psforcer
