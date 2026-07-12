#include "App.h"
#include "Catalog.h"
#include "FileUtil.h"

#include <cstdio>

namespace psforcer {

namespace {
const int kEkranGenisligi = 1920;
const int kEkranYuksekligi = 1080;
const int kSutunSayisi = 4;
}

App::App()
    : window_(NULL), renderer_(NULL), controller_(NULL), screen_(Screen::Browse), filter_(0),
      selectedVisible_(0), selectedPackage_(0), toastUntil_(0), installer_(new ManualInstaller()),
      nextJobId_(1), lastHandledJobId_(0), lastHandledCatalogJobId_(0),
      pendingItemIndex_(0), pendingPackageIndex_(0), catalogRefreshSilent_(false) {}

App::~App() { shutdown(); }

std::string App::runtimeRoot() const {
#if defined(PSFORCER_ORBIS)
    return "/data/psforcer";
#else
    return "data/psforcer";
#endif
}

std::string App::bundledPath(const std::string& relative) const {
#if defined(PSFORCER_ORBIS)
    return "/app0/" + relative;
#else
    return relative;
#endif
}

bool App::initialize(std::string& error) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) {
        error = std::string("Görüntü ve denetleyici altyapısı başlatılamadı: ") + SDL_GetError();
        return false;
    }
    window_ = SDL_CreateWindow("PSForcer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               kEkranGenisligi, kEkranYuksekligi, 0);
    if (!window_) {
        error = std::string("Uygulama penceresi oluşturulamadı: ") + SDL_GetError();
        return false;
    }
    SDL_Surface* surface = SDL_GetWindowSurface(window_);
    renderer_ = SDL_CreateSoftwareRenderer(surface);
    if (!renderer_) {
        error = std::string("Çizim altyapısı oluşturulamadı: ") + SDL_GetError();
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    if (!ui_.initialize(renderer_, error)) return false;
    SDL_JoystickEventState(SDL_ENABLE);
    if (SDL_NumJoysticks() > 0) controller_ = SDL_JoystickOpen(0);

    ensureDirectory(runtimeRoot());
    ensureDirectory(runtimeRoot() + "/indirmeler");
    ensureDirectory(runtimeRoot() + "/gorseller");

    std::string mediaError;
    mediaCache_.initialize(runtimeRoot() + "/gecici-medya", mediaError);
    ui_.setMediaResolver([this](const std::string& path) {
        return mediaCache_.resolve(path);
    });

    if (!loadCatalog(error)) return false;
    rebuildVisible();
    status_ = mediaError.empty() ? "Hazır" : "Hazır - görsel önbelleği kullanılamıyor";

    // Uygulama açıldığında GitHub'daki hafif katalog kendiliğinden yenilenir.
    // Başarısız olursa yerleşik katalog kullanılmaya devam eder.
    refreshCatalog(true);
    return true;
}

void App::shutdown() {
    downloads_.stopAndWait();
    catalogDownloads_.stopAndWait();
    mediaCache_.shutdown();
    ui_.shutdown();
    if (controller_) SDL_JoystickClose(controller_);
    controller_ = NULL;
    if (renderer_) SDL_DestroyRenderer(renderer_);
    renderer_ = NULL;
    if (window_) SDL_DestroyWindow(window_);
    window_ = NULL;
    SDL_Quit();
}

bool App::loadCatalog(std::string& error) {
    const std::string uzakKatalog = runtimeRoot() + "/katalog.json";
    if (fileExists(uzakKatalog) && CatalogLoader::loadFile(uzakKatalog, catalog_, error)) {
        status_ = "Uzak katalog yüklendi";
        return true;
    }
    error.clear();
    if (CatalogLoader::loadFile(bundledPath("assets/catalog.json"), catalog_, error)) {
        status_ = "Yerleşik katalog yüklendi";
        return true;
    }
    return false;
}

void App::rebuildVisible() {
    visible_.clear();
    for (size_t i = 0; i < catalog_.items.size(); ++i) {
        const CatalogItem& item = catalog_.items[i];
        bool include = filter_ == 0;
        if (filter_ == 1) include = CatalogLoader::itemContainsKind(item, PackageKind::Game);
        if (filter_ == 2) include = CatalogLoader::itemContainsKind(item, PackageKind::Update);
        if (filter_ == 3) include = CatalogLoader::itemContainsKind(item, PackageKind::Dlc) ||
                                    CatalogLoader::itemContainsKind(item, PackageKind::Extra);
        if (include) visible_.push_back(i);
    }
    if (visible_.empty()) selectedVisible_ = 0;
    else if (selectedVisible_ >= visible_.size()) selectedVisible_ = visible_.size() - 1;
}

void App::moveBrowse(int horizontal, int vertical) {
    if (visible_.empty()) return;
    int next = static_cast<int>(selectedVisible_) + horizontal + vertical * kSutunSayisi;
    if (next < 0) next = 0;
    if (next >= static_cast<int>(visible_.size())) next = static_cast<int>(visible_.size()) - 1;
    selectedVisible_ = static_cast<size_t>(next);
}

void App::movePackage(int delta) {
    if (visible_.empty()) return;
    const CatalogItem& item = catalog_.items[visible_[selectedVisible_]];
    if (item.packages.empty()) return;
    int next = static_cast<int>(selectedPackage_) + delta;
    if (next < 0) next = 0;
    if (next >= static_cast<int>(item.packages.size())) next = static_cast<int>(item.packages.size()) - 1;
    selectedPackage_ = static_cast<size_t>(next);
}

void App::setToast(const std::string& message, uint32_t durationMs) {
    toast_ = message;
    toastUntil_ = SDL_GetTicks() + durationMs;
}

}  // namespace psforcer
