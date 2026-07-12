#include "Ui.h"
#include "Catalog.h"
#include "PixelFont.h"

#include <SDL2/SDL_image.h>
#include <algorithm>
#include <sstream>

namespace psforcer {

namespace {
const int kEkranGenisligi = 1920;
const int kEkranYuksekligi = 1080;

Color saydam(const Color& renk, uint8_t alfa) { return Color(renk.r, renk.g, renk.b, alfa); }

Color karistir(const Color& a, const Color& b, float oran) {
    return Color(static_cast<uint8_t>(a.r + (b.r - a.r) * oran),
                 static_cast<uint8_t>(a.g + (b.g - a.g) * oran),
                 static_cast<uint8_t>(a.b + (b.b - a.b) * oran),
                 static_cast<uint8_t>(a.a + (b.a - a.a) * oran));
}

int metinGenisligi(const std::string& metin, int yukseklik) {
    const std::string guvenli = turkceyiPikselYaziyaCevir(metin);
    if (guvenli.empty()) return 0;
    const YaziOlculeri olcu = yaziOlculeri(yukseklik);
    return static_cast<int>(guvenli.size()) * olcu.ilerleme - olcu.yatay;
}
}

Ui::Ui() : renderer_(NULL) {}
Ui::~Ui() { shutdown(); }

bool Ui::initialize(SDL_Renderer* renderer, std::string& error) {
    renderer_ = renderer;
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    if ((IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & IMG_INIT_PNG) == 0) {
        error = std::string("Görsel altyapısı başlatılamadı: ") + IMG_GetError();
        return false;
    }
    return true;
}

void Ui::shutdown() {
    for (std::map<std::string, SDL_Texture*>::iterator it = textures_.begin(); it != textures_.end(); ++it)
        SDL_DestroyTexture(it->second);
    textures_.clear();
    mediaResolver_ = std::function<std::string(const std::string&)>();
    IMG_Quit();
}

void Ui::setMediaResolver(const std::function<std::string(const std::string&)>& resolver) {
    mediaResolver_ = resolver;
}

void Ui::fillRect(int x, int y, int w, int h, const Color& color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_Rect dikdortgen = {x, y, w, h};
    SDL_RenderFillRect(renderer_, &dikdortgen);
}

void Ui::strokeRect(int x, int y, int w, int h, const Color& color, int thickness) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (int i = 0; i < thickness; ++i) {
        SDL_Rect dikdortgen = {x + i, y + i, w - i * 2, h - i * 2};
        SDL_RenderDrawRect(renderer_, &dikdortgen);
    }
}

std::string Ui::asciiSafe(const std::string& value) { return turkceyiPikselYaziyaCevir(value); }

void Ui::drawText(const std::string& original, int x, int y, int height, const Color& color, int maxWidth) {
    const std::string metin = asciiSafe(original);
    const YaziOlculeri olcu = yaziOlculeri(height);
    int imlec = x;
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (size_t i = 0; i < metin.size(); ++i) {
        if (maxWidth > 0 && imlec + 5 * olcu.yatay > x + maxWidth) break;
        const uint8_t* sekil = pixelHarf(metin[i]);
        for (int satir = 0; satir < 7; ++satir) {
            const uint8_t bitler = sekil[satir];
            for (int sutun = 0; sutun < 5; ++sutun) {
                if (!(bitler & (1 << (4 - sutun)))) continue;
                SDL_Rect piksel = {imlec + sutun * olcu.yatay, y + satir * olcu.dikey,
                                   olcu.yatay, olcu.dikey};
                SDL_RenderFillRect(renderer_, &piksel);
            }
        }
        imlec += olcu.ilerleme;
    }
}

void Ui::drawWrapped(const std::string& text, int x, int y, int width, int height, int lineHeight,
                     const Color& color, int maxLines) {
    const int sinir = std::max(1, width / yaziOlculeri(height).ilerleme);
    std::istringstream kelimeler(asciiSafe(text));
    std::string kelime, satir;
    int sira = 0;
    while (kelimeler >> kelime && sira < maxLines) {
        if (satir.empty()) satir = kelime;
        else if (static_cast<int>(satir.size() + kelime.size() + 1) <= sinir) satir += " " + kelime;
        else {
            drawText(satir, x, y + sira * lineHeight, height, color, width);
            ++sira;
            satir = kelime;
        }
    }
    if (!satir.empty() && sira < maxLines) drawText(satir, x, y + sira * lineHeight, height, color, width);
}

SDL_Texture* Ui::texture(const std::string& path) {
    if (path.empty()) return NULL;

    std::map<std::string, SDL_Texture*>::iterator bulunan = textures_.find(path);
    if (bulunan != textures_.end()) return bulunan->second;

    std::string yerelYol = path;
    const bool uzak = path.compare(0, 7, "http://") == 0 || path.compare(0, 8, "https://") == 0;
    if (uzak) {
        if (!mediaResolver_) return NULL;
        yerelYol = mediaResolver_(path);
        if (yerelYol.empty()) return NULL;
    }

    SDL_Texture* yuklenen = IMG_LoadTexture(renderer_, yerelYol.c_str());
#if !defined(PSFORCER_ORBIS)
    if (!yuklenen && yerelYol.compare(0, 6, "/app0/") == 0)
        yuklenen = IMG_LoadTexture(renderer_, yerelYol.substr(6).c_str());
#endif
    if (yuklenen) textures_[path] = yuklenen;
    return yuklenen;
}

void Ui::drawImage(SDL_Texture* image, int x, int y, int w, int h, bool cover) {
    if (!image) return;
    int kaynakW = 0, kaynakH = 0;
    SDL_QueryTexture(image, NULL, NULL, &kaynakW, &kaynakH);
    SDL_Rect kaynak = {0, 0, kaynakW, kaynakH};
    if (cover && kaynakW > 0 && kaynakH > 0) {
        const float kaynakOrani = static_cast<float>(kaynakW) / kaynakH;
        const float hedefOrani = static_cast<float>(w) / h;
        if (kaynakOrani > hedefOrani) {
            kaynak.w = static_cast<int>(kaynakH * hedefOrani);
            kaynak.x = (kaynakW - kaynak.w) / 2;
        } else {
            kaynak.h = static_cast<int>(kaynakW / hedefOrani);
            kaynak.y = (kaynakH - kaynak.h) / 2;
        }
    }
    SDL_Rect hedef = {x, y, w, h};
    SDL_RenderCopy(renderer_, image, &kaynak, &hedef);
}

void Ui::drawBadge(const std::string& text, int x, int y, const Color& color) {
    const int genislik = metinGenisligi(text, 18) + 28;
    fillRect(x, y, genislik, 32, saydam(color, 230));
    drawText(text, x + 14, y + 7, 18, Color(255, 255, 255), genislik - 20);
}

std::string Ui::filterName(int filter) const {
    if (filter == 1) return "OYUNLAR";
    if (filter == 2) return "GÜNCELLEMELER";
    if (filter == 3) return "EKSTRALAR";
    return "TÜMÜ";
}

void Ui::drawBrowse(const CatalogData& catalog, const std::vector<size_t>& visible, size_t selected,
                    int filter, const std::string& status) {
    const Color arkaPlan(10, 13, 24), panel(20, 25, 42);
    fillRect(0, 0, kEkranGenisligi, kEkranYuksekligi, arkaPlan);
    fillRect(0, 0, kEkranGenisligi, 118, Color(14, 18, 31));
    fillRect(0, 116, kEkranGenisligi, 2, Color(40, 46, 70));

    drawText("PSFORCER", 58, 31, 42, Color(255, 255, 255));
    drawText("İÇERİK KÜTÜPHANESİ", 285, 47, 22, Color(125, 138, 166));

    const char* filtreler[] = {"TÜMÜ", "OYUNLAR", "GÜNCELLEMELER", "EKSTRALAR"};
    const int sekmeBaslangici = 1010;
    const int sekmeGenisligi = 200;
    const int sekmeBosluk = 15;
    for (int i = 0; i < 4; ++i) {
        const int sekmeX = sekmeBaslangici + i * (sekmeGenisligi + sekmeBosluk);
        if (i == filter) {
            fillRect(sekmeX, 32, sekmeGenisligi, 52, Color(82, 94, 226));
            strokeRect(sekmeX, 32, sekmeGenisligi, 52, Color(142, 151, 255), 2);
        }
        const int yaziW = metinGenisligi(filtreler[i], 22);
        drawText(filtreler[i], sekmeX + std::max(10, (sekmeGenisligi - yaziW) / 2), 48, 22,
                 i == filter ? Color(255, 255, 255) : Color(155, 164, 187), sekmeGenisligi - 20);
    }

    drawText(filterName(filter), 60, 148, 30, Color(236, 239, 248));
    std::ostringstream sayac;
    sayac << visible.size() << " BAŞLIK";
    drawText(sayac.str(), 60, 190, 20, Color(116, 127, 151));

    const int kartW = 420;
    const int kartH = 340;
    const int resimH = 205;
    const int bosluk = 24;
    const int satirBoslugu = 22;
    const int sutunSayisi = 4;
    const int baslangicX = 60;
    const int baslangicY = 236;

    for (size_t gorunen = 0; gorunen < visible.size(); ++gorunen) {
        const int satir = static_cast<int>(gorunen / sutunSayisi);
        if (satir > 1) break;
        const int sutun = static_cast<int>(gorunen % sutunSayisi);
        const int x = baslangicX + sutun * (kartW + bosluk);
        const int y = baslangicY + satir * (kartH + satirBoslugu);
        const CatalogItem& oge = catalog.items[visible[gorunen]];
        const bool secili = gorunen == selected;

        if (secili) fillRect(x - 8, y - 8, kartW + 16, kartH + 16, saydam(oge.accent, 170));
        fillRect(x, y, kartW, kartH, panel);

        SDL_Texture* kapak = texture(oge.media.cover);
        if (kapak) drawImage(kapak, x, y, kartW, resimH, true);
        else {
            fillRect(x, y, kartW, resimH, karistir(oge.accent, Color(8, 11, 20), 0.42f));
            fillRect(x + 24, y + 24, 92, 8, saydam(Color(255, 255, 255), 90));
            fillRect(x + 24, y + 46, 170, 5, saydam(Color(255, 255, 255), 50));
            const int psW = metinGenisligi("PS", 56);
            drawText("PS", x + (kartW - psW) / 2, y + 72, 56, Color(255, 255, 255, 190));
        }

        fillRect(x, y + resimH - 36, kartW, 36, Color(8, 10, 18, 175));
        drawBadge(oge.genre, x + 18, y + resimH - 28, oge.accent);
        drawText(oge.title, x + 22, y + 232, 26, Color(249, 250, 255), kartW - 44);
        drawText(oge.developer, x + 22, y + 274, 18, Color(151, 161, 183), kartW - 44);
        std::ostringstream bilgi;
        bilgi << oge.releaseYear << "  /  " << oge.packages.size() << " PAKET";
        drawText(bilgi.str(), x + 22, y + 307, 18, Color(108, 120, 145), kartW - 44);

        if (secili) strokeRect(x - 8, y - 8, kartW + 16, kartH + 16, Color(255, 255, 255, 230), 3);
    }

    if (visible.empty()) drawText("BU FİLTREDE İÇERİK YOK", 615, 470, 34, Color(145, 153, 176));

    fillRect(0, 1018, kEkranGenisligi, 62, Color(14, 18, 31));
    fillRect(0, 1016, kEkranGenisligi, 2, Color(40, 46, 70));
    drawText("X DETAY", 54, 1037, 20, Color(224, 228, 240));
    drawText("L1 / R1 FİLTRE", 245, 1037, 20, Color(150, 160, 184));
    drawText("ÜÇGEN YENİLE", 540, 1037, 20, Color(150, 160, 184));
    drawText(status, 1320, 1037, 18, Color(110, 202, 181), 540);
}

void Ui::drawDetail(const CatalogItem& item, size_t selectedPackage, const std::string& status) {
    fillRect(0, 0, kEkranGenisligi, kEkranYuksekligi, Color(9, 12, 22));
    SDL_Texture* kahraman = texture(item.media.hero);
    if (kahraman) drawImage(kahraman, 0, 0, kEkranGenisligi, 405, true);
    else fillRect(0, 0, kEkranGenisligi, 405, karistir(item.accent, Color(7, 10, 19), 0.60f));
    fillRect(0, 0, kEkranGenisligi, 405, Color(4, 6, 12, 92));
    fillRect(0, 300, kEkranGenisligi, 105, Color(9, 12, 22, 220));

    drawText(item.title, 72, 116, 48, Color(255, 255, 255), 1000);
    std::ostringstream ust;
    ust << item.developer << "  /  " << item.genre << "  /  " << item.releaseYear;
    drawText(ust.str(), 75, 188, 22, Color(224, 228, 239), 1100);
    drawBadge(item.packages.empty() ? "KATALOG" : packageKindName(item.packages[0].kind), 75, 238, item.accent);
    if (!item.media.trailer.empty()) drawBadge("VİDEO", 245, 238, Color(220, 76, 113));
    if (!item.media.screenshots.empty()) {
        std::ostringstream g;
        g << item.media.screenshots.size() << " GÖRSEL";
        drawBadge(g.str(), 365, 238, Color(66, 157, 217));
    }

    drawText("HAKKINDA", 72, 444, 24, Color(126, 138, 166));
    drawWrapped(item.description, 72, 486, 690, 21, 33, Color(225, 230, 241), 7);
    drawText("PAKETLER", 840, 444, 24, Color(126, 138, 166));

    for (size_t i = 0; i < item.packages.size(); ++i) {
        const PackageInfo& paket = item.packages[i];
        const int y = 490 + static_cast<int>(i) * 104;
        const bool secili = i == selectedPackage;
        if (secili) fillRect(820, y - 8, 1020, 92, saydam(item.accent, 195));
        fillRect(834, y, 992, 76, secili ? Color(31, 37, 58) : Color(20, 25, 42));
        drawBadge(packageKindName(paket.kind), 852, y + 20, item.accent);
        drawText(paket.label, 1050, y + 13, 24, Color(248, 250, 255), 430);
        std::ostringstream surum;
        surum << "SÜRÜM " << paket.version;
        if (!paket.minFirmware.empty()) surum << "  YAZILIM " << paket.minFirmware;
        drawText(surum.str(), 1050, y + 47, 17, Color(132, 143, 168), 430);
        drawText(CatalogLoader::formatBytes(paket.sizeBytes), 1580, y + 25, 20, Color(225, 230, 241), 220);
    }

    if (!item.media.screenshots.empty()) {
        drawText("GÖRSELLER", 72, 748, 22, Color(126, 138, 166));
        const size_t adet = std::min<size_t>(3, item.media.screenshots.size());
        for (size_t i = 0; i < adet; ++i) {
            const int x = 72 + static_cast<int>(i) * 228;
            const int y = 790;
            fillRect(x, y, 210, 118, Color(20, 25, 42));
            SDL_Texture* goruntu = texture(item.media.screenshots[i]);
            if (goruntu) drawImage(goruntu, x, y, 210, 118, true);
            strokeRect(x, y, 210, 118, Color(65, 73, 96), 2);
        }
    }

    fillRect(0, 1018, kEkranGenisligi, 62, Color(14, 18, 31));
    drawText("DAİRE GERİ", 54, 1037, 20, Color(224, 228, 240));
    drawText("KARE İNDİR", 280, 1037, 20, Color(224, 228, 240));
    drawText(status, 1130, 1037, 18, Color(110, 202, 181), 730);
}

void Ui::drawDownload(const DownloadSnapshot& snapshot) {
    if (snapshot.state == DownloadState::Idle) return;
    fillRect(0, 0, kEkranGenisligi, kEkranYuksekligi, Color(0, 0, 0, 105));
    const int x = 460, y = 380, w = 1000, h = 250;
    fillRect(x, y, w, h, Color(20, 25, 42, 250));
    strokeRect(x, y, w, h, Color(91, 84, 218), 3);
    drawText(snapshot.label, x + 48, y + 42, 30, Color(250, 251, 255), w - 96);

    std::string durum = "İNDİRİLİYOR";
    if (snapshot.state == DownloadState::Verifying) durum = "BÜTÜNLÜK DENETLENİYOR";
    else if (snapshot.state == DownloadState::Completed) durum = "TAMAMLANDI";
    else if (snapshot.state == DownloadState::Failed) durum = "HATA";
    else if (snapshot.state == DownloadState::Cancelled) durum = "İPTAL EDİLDİ";
    drawText(durum, x + 48, y + 92, 20, Color(137, 148, 173));

    const float oran = snapshot.total > 0 ? std::min(1.0f, static_cast<float>(snapshot.downloaded) / snapshot.total) : 0.0f;
    fillRect(x + 48, y + 140, w - 96, 22, Color(43, 50, 72));
    fillRect(x + 48, y + 140, static_cast<int>((w - 96) * oran), 22, Color(103, 93, 235));
    std::ostringstream miktar;
    miktar << CatalogLoader::formatBytes(snapshot.downloaded);
    if (snapshot.total > 0) miktar << " / " << CatalogLoader::formatBytes(snapshot.total);
    drawText(miktar.str(), x + 48, y + 180, 18, Color(205, 211, 225));
    if ((snapshot.state == DownloadState::Failed || snapshot.state == DownloadState::Cancelled) &&
    !snapshot.error.empty()) {
    drawText(snapshot.error, x + 360, y + 180, 17, Color(239, 103, 121), 550);
    } else if (snapshot.state == DownloadState::Running && snapshot.bytesPerSecond > 0) {
    std::ostringstream hiz;
    hiz << CatalogLoader::formatBytes(snapshot.bytesPerSecond) << "/S";
    drawText(hiz.str(), x + 710, y + 180, 18, Color(110, 202, 181), 200);
    } else if (!snapshot.status.empty()) {
    drawText(snapshot.status, x + 360, y + 180, 17, Color(137, 148, 173), 550);
    }
}

void Ui::drawToast(const std::string& message) {
    if (message.empty()) return;
    fillRect(420, 920, 1080, 62, Color(24, 31, 50, 245));
    strokeRect(420, 920, 1080, 62, Color(92, 203, 174), 2);
    drawText(message, 452, 941, 20, Color(238, 242, 249), 1016);
}

}  // namespace psforcer
