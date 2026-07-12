#include "Ui.h"
#include "Catalog.h"

#include <SDL2/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace psforcer {

namespace {
const int kScreenWidth = 1920;
const int kScreenHeight = 1080;

const uint64_t kGlyphBits[95] = {
    0x000000000ULL, 0x39CE639C0ULL, 0x7BDE00000ULL, 0x01FFFFFC0ULL, 0x3BDEF79CCULL,
    0x639FFFDE0ULL, 0x3BDFFFFE0ULL, 0x318C00000ULL, 0x39CC631C6ULL, 0x31CE739CCULL,
    0x33DEF3000ULL, 0x019FFB180ULL, 0x000E73000ULL, 0x01CE00000ULL, 0x000E73800ULL,
    0x18CE73318ULL, 0x3BDFFFFC0ULL, 0x7BCE73BE0ULL, 0x7BCE77BC0ULL, 0x3BCE7FFC0ULL,
    0x39DEFFCC0ULL, 0x7BDE37BC0ULL, 0x3BDEFFFC0ULL, 0x7BCE73980ULL, 0x3BDEFFFC0ULL,
    0x3BDFFFFC0ULL, 0x01CE739C0ULL, 0x01CE73980ULL, 0x01FEFBC00ULL, 0x03FFF8000ULL,
    0x03DFFF800ULL, 0x3BCE63180ULL, 0x3BFFFFFEFULL, 0x39DEF7FE0ULL, 0x7BFFFFFE0ULL,
    0x3BD8C73C0ULL, 0x7BFFFFFC0ULL, 0x7FFEF63E0ULL, 0x7FFEF6300ULL, 0x3BD8FFFE0ULL,
    0x7FFFFFFE0ULL, 0x7BCE73BC0ULL, 0x39C637BC0ULL, 0x7FFEF7FE0ULL, 0x6318C63E0ULL,
    0x7FFFFFFE0ULL, 0x7FFFFFFE0ULL, 0x3BFFFFFC0ULL, 0x7BFFF6300ULL, 0x3BFFFFFC6ULL,
    0x7BFFF7FE0ULL, 0x3BDE7FFC0ULL, 0x7FEE739C0ULL, 0x7FFFFFFC0ULL, 0x7FFEF79C0ULL,
    0x6FFFFFFE0ULL, 0x7FEEF7BE0ULL, 0x7FFE739C0ULL, 0x7FEEF73E0ULL, 0x398C631CEULL,
    0x630C738C6ULL, 0x39CE739CEULL, 0x3BFF00000ULL, 0x0000FFC00ULL, 0x218000000ULL,
    0x03DFFFFE0ULL, 0x631EFFFFEULL, 0x03DEF79C0ULL, 0x18DEF7BDEULL, 0x03DFFFDC0ULL,
    0x39DE6318CULL, 0x3FFFFFFCEULL, 0x631EF7BDEULL, 0x39DE73BFFULL, 0x3BCE739DEULL,
    0x631FF7BFFULL, 0x738C631EFULL, 0x03FFFFFE0ULL, 0x03DEF7BC0ULL, 0x03DFFFDC0ULL,
    0x7BFFFFB18ULL, 0x7BDEF78C6ULL, 0x01EF63180ULL, 0x03DEF79C0ULL, 0x33DE631C0ULL,
    0x03DEF7BC0ULL, 0x03FEF39C0ULL, 0x03FFFFFC0ULL, 0x03DEF7FE0ULL, 0x7FFE73BDCULL,
    0x03CEF7BC0ULL, 0x39DEF39CEULL, 0x318C6318CULL, 0x71CE73B9CULL, 0x001F70000ULL
};
const int kGlyphWidth = 5;
const int kGlyphHeight = 7;

Color withAlpha(const Color& color, uint8_t alpha) { return Color(color.r, color.g, color.b, alpha); }
Color mix(const Color& a, const Color& b, float amount) {
    return Color(static_cast<uint8_t>(a.r + (b.r - a.r) * amount),
                 static_cast<uint8_t>(a.g + (b.g - a.g) * amount),
                 static_cast<uint8_t>(a.b + (b.b - a.b) * amount),
                 static_cast<uint8_t>(a.a + (b.a - a.a) * amount));
}
}  // namespace

Ui::Ui() : renderer_(NULL) {}
Ui::~Ui() { shutdown(); }

bool Ui::initialize(SDL_Renderer* renderer, std::string& error) {
    renderer_ = renderer;
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    if ((IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & IMG_INIT_PNG) == 0) {
        error = std::string("SDL_image init failed: ") + IMG_GetError();
        return false;
    }
    return true;
}

void Ui::shutdown() {
    for (std::map<std::string, SDL_Texture*>::iterator it = textures_.begin(); it != textures_.end(); ++it)
        SDL_DestroyTexture(it->second);
    textures_.clear();
    IMG_Quit();
}

void Ui::fillRect(int x, int y, int w, int h, const Color& color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(renderer_, &rect);
}

void Ui::strokeRect(int x, int y, int w, int h, const Color& color, int thickness) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (int i = 0; i < thickness; ++i) {
        SDL_Rect rect = {x + i, y + i, w - i * 2, h - i * 2};
        SDL_RenderDrawRect(renderer_, &rect);
    }
}

std::string Ui::asciiSafe(const std::string& value) {
    std::string safe;
    for (size_t i = 0; i < value.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (c >= 32 && c <= 126) safe.push_back(static_cast<char>(c));
        else if ((c & 0xC0) != 0x80) safe.push_back('?');
    }
    return safe;
}

void Ui::drawText(const std::string& original, int x, int y, int height, const Color& color, int maxWidth) {
    const std::string text = asciiSafe(original);
    const int scale = std::max(1, height / kGlyphHeight);
    const int glyphWidth = (kGlyphWidth + 1) * scale;
    int cursor = x;
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (size_t i = 0; i < text.size(); ++i) {
        if (maxWidth > 0 && cursor + glyphWidth > x + maxWidth) break;
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 32 || c > 126) c = '?';
        const uint64_t glyph = kGlyphBits[c - 32];
        for (int row = 0; row < kGlyphHeight; ++row) {
            const int shift = (kGlyphHeight - 1 - row) * kGlyphWidth;
            const uint8_t rowBits = static_cast<uint8_t>((glyph >> shift) & 0x1F);
            int runStart = -1;
            for (int column = 0; column <= kGlyphWidth; ++column) {
                const bool on = column < kGlyphWidth && (rowBits & (1 << (kGlyphWidth - 1 - column)));
                if (on && runStart < 0) runStart = column;
                if ((!on || column == kGlyphWidth) && runStart >= 0) {
                    SDL_Rect pixelRun = {cursor + runStart * scale, y + row * scale,
                                         (column - runStart) * scale, scale};
                    SDL_RenderFillRect(renderer_, &pixelRun);
                    runStart = -1;
                }
            }
        }
        cursor += glyphWidth;
    }
}

void Ui::drawWrapped(const std::string& text, int x, int y, int width, int height, int lineHeight,
                     const Color& color, int maxLines) {
    const int glyphWidth = (kGlyphWidth + 1) * std::max(1, height / kGlyphHeight);
    const int maxChars = std::max(1, width / std::max(1, glyphWidth));
    std::istringstream words(asciiSafe(text));
    std::string word;
    std::string line;
    int lineIndex = 0;
    while (words >> word && lineIndex < maxLines) {
        if (line.empty()) line = word;
        else if (static_cast<int>(line.size() + word.size() + 1) <= maxChars) line += " " + word;
        else {
            drawText(line, x, y + lineIndex * lineHeight, height, color, width);
            ++lineIndex;
            line = word;
        }
    }
    if (!line.empty() && lineIndex < maxLines) drawText(line, x, y + lineIndex * lineHeight, height, color, width);
}

SDL_Texture* Ui::texture(const std::string& path) {
    if (path.empty() || path.compare(0, 7, "http://") == 0 || path.compare(0, 8, "https://") == 0) return NULL;
    std::map<std::string, SDL_Texture*>::iterator existing = textures_.find(path);
    if (existing != textures_.end()) return existing->second;
    SDL_Texture* loaded = IMG_LoadTexture(renderer_, path.c_str());
#if !defined(PSFORCER_ORBIS)
    if (!loaded && path.compare(0, 6, "/app0/") == 0) loaded = IMG_LoadTexture(renderer_, path.substr(6).c_str());
#endif
    if (loaded) {
        SDL_SetTextureBlendMode(loaded, SDL_BLENDMODE_BLEND);
        textures_[path] = loaded;
    }
    return loaded;
}

void Ui::drawImage(SDL_Texture* image, int x, int y, int w, int h, bool cover) {
    if (!image) return;
    int sourceWidth = 0, sourceHeight = 0;
    SDL_QueryTexture(image, NULL, NULL, &sourceWidth, &sourceHeight);
    SDL_Rect source = {0, 0, sourceWidth, sourceHeight};
    if (cover && sourceWidth > 0 && sourceHeight > 0) {
        const float sourceRatio = static_cast<float>(sourceWidth) / sourceHeight;
        const float targetRatio = static_cast<float>(w) / h;
        if (sourceRatio > targetRatio) {
            const int croppedWidth = static_cast<int>(sourceHeight * targetRatio);
            source.x = (sourceWidth - croppedWidth) / 2;
            source.w = croppedWidth;
        } else {
            const int croppedHeight = static_cast<int>(sourceWidth / targetRatio);
            source.y = (sourceHeight - croppedHeight) / 2;
            source.h = croppedHeight;
        }
    }
    SDL_Rect destination = {x, y, w, h};
    SDL_RenderCopy(renderer_, image, &source, &destination);
}

void Ui::drawBadge(const std::string& text, int x, int y, const Color& color) {
    const int width = static_cast<int>(text.size()) * 15 + 28;
    fillRect(x, y, width, 34, withAlpha(color, 220));
    drawText(text, x + 14, y + 7, 20, Color(255, 255, 255), width - 20);
}

std::string Ui::filterName(int filter) const {
    switch (filter) {
        case 1: return "OYUNLAR";
        case 2: return "GUNCELLEMELER";
        case 3: return "EKSTRALAR";
        default: return "TUMU";
    }
}

void Ui::drawBrowse(const CatalogData& catalog, const std::vector<size_t>& visible, size_t selected,
                    int filter, const std::string& status) {
    const Color background(10, 13, 24), panel(20, 25, 42);
    fillRect(0, 0, kScreenWidth, kScreenHeight, background);
    fillRect(0, 0, kScreenWidth, 112, Color(14, 18, 31));
    drawText("PSFORCER", 58, 33, 46, Color(255, 255, 255));
    drawText("CONTENT LIBRARY", 330, 48, 24, Color(125, 138, 166));

    const char* filters[] = {"TUMU", "OYUNLAR", "GUNCELLEMELER", "EKSTRALAR"};
    int filterX = 1040;
    for (int i = 0; i < 4; ++i) {
        const int width = static_cast<int>(std::string(filters[i]).size()) * 14 + 34;
        if (i == filter) fillRect(filterX - 12, 35, width, 44, Color(93, 83, 220));
        drawText(filters[i], filterX, 47, 22, i == filter ? Color(255, 255, 255) : Color(145, 153, 176));
        filterX += width + 18;
    }

    drawText(filterName(filter), 60, 144, 32, Color(236, 239, 248));
    std::ostringstream count;
    count << visible.size() << " BASLIK";
    drawText(count.str(), 60, 188, 20, Color(116, 127, 151));

    const int cardWidth = 320, cardHeight = 400, gap = 28, columns = 5, startX = 60, startY = 236;
    for (size_t visibleIndex = 0; visibleIndex < visible.size(); ++visibleIndex) {
        const int row = static_cast<int>(visibleIndex / columns);
        if (row > 1) break;
        const int column = static_cast<int>(visibleIndex % columns);
        const int x = startX + column * (cardWidth + gap), y = startY + row * (cardHeight + 30);
        const CatalogItem& item = catalog.items[visible[visibleIndex]];
        const bool isSelected = visibleIndex == selected;
        if (isSelected) fillRect(x - 8, y - 8, cardWidth + 16, cardHeight + 16, withAlpha(item.accent, 180));
        fillRect(x, y, cardWidth, cardHeight, panel);
        SDL_Texture* cover = texture(item.media.cover);
        if (cover) drawImage(cover, x, y, cardWidth, 270, true);
        else {
            fillRect(x, y, cardWidth, 270, mix(item.accent, Color(8, 11, 20), 0.42f));
            fillRect(x + 22, y + 22, 84, 9, withAlpha(Color(255, 255, 255), 90));
            fillRect(x + 22, y + 43, 150, 5, withAlpha(Color(255, 255, 255), 50));
            drawText("PS", x + 118, y + 103, 66, Color(255, 255, 255, 190));
        }
        fillRect(x, y + 238, cardWidth, 32, Color(8, 10, 18, 170));
        drawBadge(item.genre, x + 16, y + 222, item.accent);
        drawText(item.title, x + 20, y + 296, 28, Color(249, 250, 255), cardWidth - 40);
        drawText(item.developer, x + 20, y + 337, 19, Color(133, 143, 166), cardWidth - 40);
        std::ostringstream meta;
        meta << item.releaseYear << "  /  " << item.packages.size() << " PAKET";
        drawText(meta.str(), x + 20, y + 370, 18, Color(102, 113, 138), cardWidth - 40);
        if (isSelected) strokeRect(x - 8, y - 8, cardWidth + 16, cardHeight + 16, Color(255, 255, 255, 220), 3);
    }
    if (visible.empty()) drawText("BU FILTREDE ICERIK YOK", 650, 465, 36, Color(145, 153, 176));

    fillRect(0, 1018, kScreenWidth, 62, Color(14, 18, 31));
    drawText("X DETAY", 54, 1037, 22, Color(224, 228, 240));
    drawText("L1/R1 FILTRE", 210, 1037, 22, Color(150, 160, 184));
    drawText("TRIANGLE KATALOGU YENILE", 442, 1037, 22, Color(150, 160, 184));
    drawText(status, 1110, 1037, 20, Color(110, 202, 181), 750);
}

void Ui::drawDetail(const CatalogItem& item, size_t selectedPackage, const std::string& status) {
    fillRect(0, 0, kScreenWidth, kScreenHeight, Color(9, 12, 22));
    SDL_Texture* hero = texture(item.media.hero);
    if (hero) drawImage(hero, 0, 0, kScreenWidth, 405, true);
    else fillRect(0, 0, kScreenWidth, 405, mix(item.accent, Color(7, 10, 19), 0.60f));
    fillRect(0, 0, kScreenWidth, 405, Color(4, 6, 12, 92));
    fillRect(0, 300, kScreenWidth, 105, Color(9, 12, 22, 220));

    drawText(item.title, 72, 116, 58, Color(255, 255, 255), 1000);
    std::ostringstream info;
    info << item.developer << "  /  " << item.genre << "  /  " << item.releaseYear;
    drawText(info.str(), 75, 189, 25, Color(224, 228, 239), 1100);
    drawBadge(item.packages.empty() ? "KATALOG" : packageKindName(item.packages[0].kind), 75, 238, item.accent);
    if (!item.media.trailer.empty()) drawBadge("VIDEO", 230, 238, Color(220, 76, 113));
    if (!item.media.screenshots.empty()) {
        std::ostringstream shots;
        shots << item.media.screenshots.size() << " GORSEL";
        drawBadge(shots.str(), 335, 238, Color(66, 157, 217));
    }

    drawText("HAKKINDA", 72, 444, 25, Color(126, 138, 166));
    drawWrapped(item.description, 72, 486, 690, 24, 35, Color(225, 230, 241), 7);
    drawText("PAKETLER", 840, 444, 25, Color(126, 138, 166));
    for (size_t i = 0; i < item.packages.size(); ++i) {
        const PackageInfo& package = item.packages[i];
        const int y = 490 + static_cast<int>(i) * 104;
        const bool isSelected = i == selectedPackage;
        if (isSelected) fillRect(820, y - 8, 1020, 92, withAlpha(item.accent, 195));
        fillRect(834, y, 992, 76, isSelected ? Color(31, 37, 58) : Color(20, 25, 42));
        drawBadge(packageKindName(package.kind), 852, y + 20, item.accent);
        drawText(package.label, 1038, y + 13, 27, Color(248, 250, 255), 445);
        std::ostringstream version;
        version << "v" << package.version;
        if (!package.minFirmware.empty()) version << "  FW " << package.minFirmware;
        drawText(version.str(), 1038, y + 47, 18, Color(132, 143, 168), 440);
        drawText(CatalogLoader::formatBytes(package.sizeBytes), 1570, y + 25, 23, Color(225, 230, 241), 220);
    }

    fillRect(0, 1018, kScreenWidth, 62, Color(14, 18, 31));
    drawText("CIRCLE GERI", 54, 1037, 22, Color(224, 228, 240));
    drawText("SQUARE INDIR", 252, 1037, 22, Color(224, 228, 240));
    drawText(status, 930, 1037, 20, Color(110, 202, 181), 900);
}

void Ui::drawDownload(const DownloadSnapshot& snapshot) {
    if (snapshot.state == DownloadState::Idle) return;
    fillRect(0, 0, kScreenWidth, kScreenHeight, Color(0, 0, 0, 105));
    const int x = 460, y = 380, w = 1000, h = 250;
    fillRect(x, y, w, h, Color(20, 25, 42, 250));
    strokeRect(x, y, w, h, Color(91, 84, 218), 3);
    drawText(snapshot.label, x + 48, y + 42, 34, Color(250, 251, 255), w - 96);

    std::string stateText = "INDIRILIYOR";
    if (snapshot.state == DownloadState::Verifying) stateText = "SHA-256 DOGRULANIYOR";
    else if (snapshot.state == DownloadState::Completed) stateText = "TAMAMLANDI";
    else if (snapshot.state == DownloadState::Failed) stateText = "HATA";
    else if (snapshot.state == DownloadState::Cancelled) stateText = "IPTAL EDILDI";
    drawText(stateText, x + 48, y + 92, 22, Color(137, 148, 173));

    const float ratio = snapshot.total > 0 ? std::min(1.0f, static_cast<float>(snapshot.downloaded) / snapshot.total) : 0.0f;
    fillRect(x + 48, y + 140, w - 96, 22, Color(43, 50, 72));
    fillRect(x + 48, y + 140, static_cast<int>((w - 96) * ratio), 22, Color(103, 93, 235));
    std::ostringstream amount;
    amount << CatalogLoader::formatBytes(snapshot.downloaded);
    if (snapshot.total > 0) amount << " / " << CatalogLoader::formatBytes(snapshot.total);
    drawText(amount.str(), x + 48, y + 180, 21, Color(205, 211, 225));
    if (!snapshot.error.empty()) drawText(snapshot.error, x + 360, y + 180, 19, Color(239, 103, 121), 550);
}

void Ui::drawToast(const std::string& message) {
    if (message.empty()) return;
    fillRect(520, 924, 880, 58, Color(24, 31, 50, 245));
    strokeRect(520, 924, 880, 58, Color(92, 203, 174), 2);
    drawText(message, 550, 941, 22, Color(238, 242, 249), 820);
}

}  // namespace psforcer
