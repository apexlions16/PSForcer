#include "Ui.h"
#include "Catalog.h"

#include <algorithm>
#include <sstream>

namespace psforcer {

void Ui::drawDetailV016(const CatalogItem& item, size_t selectedPackage, const std::string& status) {
    const int screenWidth = 1920;
    const int screenHeight = 1080;
    const int headerHeight = 500;
    const Color panel(15, 20, 34, 238);

    fillRect(0, 0, screenWidth, screenHeight, Color(8, 11, 20));

    SDL_Texture* hero = texture(item.media.hero);
    SDL_Texture* cover = texture(item.media.cover);
    if (hero) {
        drawImage(hero, 0, 0, screenWidth, headerHeight, true);
    } else {
        fillRect(0, 0, screenWidth, headerHeight,
                 Color(item.accent.r / 2, item.accent.g / 2, item.accent.b / 2));
    }

    fillRect(0, 0, screenWidth, headerHeight, Color(3, 5, 10, 82));
    for (int i = 0; i < 13; ++i) {
        const int alpha = std::max(28, 214 - i * 15);
        fillRect(i * 90, 0, 92, headerHeight,
                 Color(4, 6, 12, static_cast<uint8_t>(alpha)));
    }
    fillRect(0, 330, screenWidth, 170, Color(7, 10, 18, 205));

    drawText(item.title, 74, 76, 72, Color(255, 255, 255), 1120);
    std::ostringstream metadata;
    metadata << item.developer << "  /  " << item.genre << "  /  " << item.releaseYear;
    drawText(metadata.str(), 78, 188, 26, Color(232, 236, 246), 1200);

    drawBadge(item.packages.empty() ? "KATALOG" : packageKindName(item.packages[0].kind),
              78, 248, item.accent);
    if (!item.media.trailer.empty()) {
        drawBadge("VİDEO", 248, 248, Color(220, 76, 113));
    }
    if (!item.media.screenshots.empty()) {
        std::ostringstream count;
        count << item.media.screenshots.size() << " GÖRSEL";
        drawBadge(count.str(), 368, 248, Color(66, 157, 217));
    }

    if (cover) {
        fillRect(1366, 62, 488, 292, Color(7, 10, 18, 225));
        drawImage(cover, 1378, 74, 464, 268, true);
        strokeRect(1366, 62, 488, 292, Color(255, 255, 255, 120), 2);
    }

    fillRect(0, 420, screenWidth, 598, Color(8, 11, 20, 248));
    fillRect(48, 438, 720, 510, panel);
    fillRect(800, 438, 1072, 510, panel);

    drawText("HAKKINDA", 72, 466, 24, Color(132, 145, 174));
    drawWrapped(item.description, 72, 508, 650, 22, 35,
                Color(226, 231, 242), 5);

    drawText("PAKETLER", 832, 466, 24, Color(132, 145, 174));
    for (size_t i = 0; i < item.packages.size(); ++i) {
        const PackageInfo& package = item.packages[i];
        const int y = 510 + static_cast<int>(i) * 100;
        const bool selected = i == selectedPackage;
        if (selected) {
            fillRect(816, y - 8, 1028, 88,
                     Color(item.accent.r, item.accent.g, item.accent.b, 205));
        }
        fillRect(830, y, 1000, 72,
                 selected ? Color(31, 38, 60) : Color(20, 26, 43));
        drawBadge(packageKindName(package.kind), 850, y + 18, item.accent);
        drawText(package.label, 1048, y + 11, 25,
                 Color(248, 250, 255), 440);

        std::ostringstream version;
        version << "SÜRÜM " << package.version;
        if (!package.minFirmware.empty()) {
            version << "  YAZILIM " << package.minFirmware;
        }
        drawText(version.str(), 1048, y + 45, 17,
                 Color(137, 148, 174), 440);
        drawText(CatalogLoader::formatBytes(package.sizeBytes),
                 1590, y + 23, 21, Color(229, 233, 243), 210);
    }

    if (!item.media.screenshots.empty()) {
        drawText("GÖRSELLER", 72, 722, 22, Color(132, 145, 174));
        const size_t count = std::min<size_t>(3, item.media.screenshots.size());
        for (size_t i = 0; i < count; ++i) {
            const int x = 72 + static_cast<int>(i) * 222;
            const int y = 760;
            fillRect(x, y, 204, 115, Color(22, 28, 46));

            SDL_Texture* image = texture(item.media.screenshots[i]);
            if (!image) image = hero;
            if (!image) image = cover;
            if (image) {
                drawImage(image, x, y, 204, 115, true);
            } else {
                fillRect(x, y, 204, 115,
                         Color(item.accent.r / 2, item.accent.g / 2, item.accent.b / 2));
                drawText("GÖRSEL", x + 58, y + 46, 18,
                         Color(235, 239, 248, 180), 100);
            }
            strokeRect(x, y, 204, 115, Color(105, 119, 151), 2);
        }
    }

    fillRect(0, 1018, screenWidth, 62, Color(14, 18, 31));
    drawText("DAİRE GERİ", 54, 1037, 20, Color(224, 228, 240));
    drawText("KARE İNDİR", 280, 1037, 20, Color(224, 228, 240));
    drawText(status, 1130, 1037, 18, Color(110, 202, 181), 730);
}

}  // namespace psforcer
