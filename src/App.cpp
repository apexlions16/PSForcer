#include "App.h"
#include "Catalog.h"
#include "FileUtil.h"

#include <SDL2/SDL_image.h>
#include <cstdio>
#include <sstream>

namespace psforcer {

namespace {
const int kScreenWidth = 1920;
const int kScreenHeight = 1080;
const int kGridColumns = 5;

bool isFinal(DownloadState state) {
    return state == DownloadState::Completed || state == DownloadState::Failed || state == DownloadState::Cancelled;
}
}

App::App()
    : window_(NULL), renderer_(NULL), controller_(NULL), screen_(Screen::Browse), filter_(0),
      selectedVisible_(0), selectedPackage_(0), toastUntil_(0), installer_(new ManualInstaller()),
      nextJobId_(1), lastHandledJobId_(0), pendingCatalog_(false), pendingItemIndex_(0), pendingPackageIndex_(0) {}

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
        error = std::string("SDL_Init failed: ") + SDL_GetError();
        return false;
    }
    window_ = SDL_CreateWindow("PSForcer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               kScreenWidth, kScreenHeight, 0);
    if (!window_) {
        error = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
        return false;
    }
    SDL_Surface* surface = SDL_GetWindowSurface(window_);
    renderer_ = SDL_CreateSoftwareRenderer(surface);
    if (!renderer_) {
        error = std::string("SDL_CreateSoftwareRenderer failed: ") + SDL_GetError();
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    if (!ui_.initialize(renderer_, error)) return false;

    SDL_JoystickEventState(SDL_ENABLE);
    if (SDL_NumJoysticks() > 0) controller_ = SDL_JoystickOpen(0);

    ensureDirectory(runtimeRoot());
    ensureDirectory(runtimeRoot() + "/downloads");
    ensureDirectory(runtimeRoot() + "/media");

    if (!loadCatalog(error)) return false;
    rebuildVisible();
    status_ = "Hazir";
    return true;
}

void App::shutdown() {
    downloads_.cancel();
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
    const std::string remoteCatalog = runtimeRoot() + "/catalog.json";
    if (fileExists(remoteCatalog) && CatalogLoader::loadFile(remoteCatalog, catalog_, error)) {
        status_ = "Remote katalog yuklendi";
        return true;
    }
    error.clear();
    if (CatalogLoader::loadFile(bundledPath("assets/catalog.json"), catalog_, error)) {
        status_ = "Dahili katalog yuklendi";
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
    int next = static_cast<int>(selectedVisible_) + horizontal + vertical * kGridColumns;
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

void App::startPackageDownload() {
    if (downloads_.busy() || visible_.empty()) {
        if (downloads_.busy()) setToast("Bir indirme zaten calisiyor");
        return;
    }
    const size_t itemIndex = visible_[selectedVisible_];
    const CatalogItem& item = catalog_.items[itemIndex];
    if (item.packages.empty() || selectedPackage_ >= item.packages.size()) return;
    const PackageInfo& package = item.packages[selectedPackage_];
    if (package.url.empty()) {
        setToast("Bu demo paketine henuz bir URL eklenmedi");
        return;
    }

    const std::string baseName = sanitizeFileName(item.id + "-" + package.id + "-" + package.version + ".pkg");
    pendingFinalPath_ = runtimeRoot() + "/downloads/" + baseName;
    DownloadRequest request;
    request.jobId = nextJobId_++;
    request.id = package.id;
    request.label = item.title + " - " + package.label;
    request.url = package.url;
    request.destination = pendingFinalPath_ + ".part";
    request.sha256 = package.sha256;
    request.resume = true;

    std::string error;
    if (!downloads_.start(request, error)) {
        setToast(error);
        return;
    }
    pendingCatalog_ = false;
    pendingItemIndex_ = itemIndex;
    pendingPackageIndex_ = selectedPackage_;
    status_ = "Indirme baslatildi";
}

void App::refreshCatalog() {
    if (downloads_.busy()) {
        setToast("Katalog yenilemek icin mevcut indirmenin bitmesini bekleyin");
        return;
    }
    std::string url = readFirstLine(runtimeRoot() + "/manifest_url.txt");
    if (url.empty()) url = readFirstLine(bundledPath("assets/manifest_url.txt"));
    if (url.empty()) {
        setToast("/data/psforcer/manifest_url.txt icine katalog URL'si yazin", 6500);
        return;
    }
    DownloadRequest request;
    request.jobId = nextJobId_++;
    request.id = "catalog-refresh";
    request.label = "Katalog yenileniyor";
    request.url = url;
    request.destination = runtimeRoot() + "/catalog.json.part";
    request.resume = false;

    std::string error;
    if (!downloads_.start(request, error)) {
        setToast(error);
        return;
    }
    pendingCatalog_ = true;
    pendingFinalPath_ = runtimeRoot() + "/catalog.json";
    status_ = "Katalog indiriliyor";
}

void App::processDownloadCompletion() {
    const DownloadSnapshot snapshot = downloads_.snapshot();
    if (!isFinal(snapshot.state) || snapshot.jobId == 0 || snapshot.jobId == lastHandledJobId_) return;
    lastHandledJobId_ = snapshot.jobId;

    if (snapshot.state == DownloadState::Failed || snapshot.state == DownloadState::Cancelled) {
        status_ = snapshot.state == DownloadState::Cancelled ? "Indirme iptal edildi" : "Indirme basarisiz";
        setToast(snapshot.error.empty() ? status_ : snapshot.error, 6000);
        downloads_.reset();
        return;
    }

    if (pendingCatalog_) {
        std::remove(pendingFinalPath_.c_str());
        if (std::rename(snapshot.destination.c_str(), pendingFinalPath_.c_str()) != 0) {
            setToast("Katalog dosyasi etkinlestirilemedi");
            downloads_.reset();
            return;
        }
        std::string error;
        if (loadCatalog(error)) {
            selectedVisible_ = 0;
            selectedPackage_ = 0;
            rebuildVisible();
            setToast("Katalog yenilendi");
            status_ = "Katalog guncel";
        } else {
            setToast("Yeni katalog gecersiz: " + error, 7000);
        }
        downloads_.reset();
        return;
    }

    std::remove(pendingFinalPath_.c_str());
    if (std::rename(snapshot.destination.c_str(), pendingFinalPath_.c_str()) != 0) {
        setToast("Dogrulanan paket yeniden adlandirilamadi");
        downloads_.reset();
        return;
    }
    if (pendingItemIndex_ >= catalog_.items.size() ||
        pendingPackageIndex_ >= catalog_.items[pendingItemIndex_].packages.size()) {
        setToast("Indirme katalog kaydiyla eslestirilemedi");
        downloads_.reset();
        return;
    }

    const CatalogItem& item = catalog_.items[pendingItemIndex_];
    const PackageInfo& package = item.packages[pendingPackageIndex_];
    const InstallOutcome outcome = installer_->requestInstall(item, package, pendingFinalPath_);
    if (outcome.result == InstallResult::Installed && package.deleteAfterInstall) {
        std::remove(pendingFinalPath_.c_str());
        status_ = "Kuruldu ve paket silindi";
    } else if (outcome.result == InstallResult::ReadyForManualInstall) {
        status_ = "Paket kurulum icin hazir";
    } else {
        status_ = "Kurulum teslimi basarisiz";
    }
    setToast(outcome.message, 6500);
    downloads_.reset();
}

void App::handleAction(Action action, bool& running) {
    if (action == Action::Exit) {
        running = false;
        return;
    }
    if (action == Action::Refresh) {
        refreshCatalog();
        return;
    }
    if (screen_ == Screen::Browse) {
        if (action == Action::Left) moveBrowse(-1, 0);
        else if (action == Action::Right) moveBrowse(1, 0);
        else if (action == Action::Up) moveBrowse(0, -1);
        else if (action == Action::Down) moveBrowse(0, 1);
        else if (action == Action::FilterPrevious) {
            filter_ = (filter_ + 3) % 4;
            selectedVisible_ = 0;
            rebuildVisible();
        } else if (action == Action::FilterNext) {
            filter_ = (filter_ + 1) % 4;
            selectedVisible_ = 0;
            rebuildVisible();
        } else if (action == Action::Confirm && !visible_.empty()) {
            screen_ = Screen::Detail;
            selectedPackage_ = 0;
        }
    } else {
        if (action == Action::Back) screen_ = Screen::Browse;
        else if (action == Action::Up) movePackage(-1);
        else if (action == Action::Down) movePackage(1);
        else if (action == Action::Download || action == Action::Confirm) startPackageDownload();
    }
}

void App::processEvents(bool& running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
            continue;
        }
        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_UP: handleAction(Action::Up, running); break;
                case SDLK_DOWN: handleAction(Action::Down, running); break;
                case SDLK_LEFT: handleAction(Action::Left, running); break;
                case SDLK_RIGHT: handleAction(Action::Right, running); break;
                case SDLK_RETURN: handleAction(Action::Confirm, running); break;
                case SDLK_ESCAPE: handleAction(Action::Back, running); break;
                case SDLK_x: handleAction(Action::Download, running); break;
                case SDLK_r: handleAction(Action::Refresh, running); break;
                case SDLK_q: handleAction(Action::Exit, running); break;
                case SDLK_PAGEUP: handleAction(Action::FilterPrevious, running); break;
                case SDLK_PAGEDOWN: handleAction(Action::FilterNext, running); break;
            }
        }
        if (event.type == SDL_JOYBUTTONDOWN) {
            switch (event.jbutton.button) {
                case 0: handleAction(Action::Confirm, running); break;
                case 1: handleAction(Action::Back, running); break;
                case 2: handleAction(Action::Download, running); break;
                case 3: handleAction(Action::Refresh, running); break;
                case 4: handleAction(Action::FilterPrevious, running); break;
                case 5: handleAction(Action::FilterNext, running); break;
                case 9: handleAction(Action::Exit, running); break;
                case 13: handleAction(Action::Up, running); break;
                case 14: handleAction(Action::Down, running); break;
                case 15: handleAction(Action::Left, running); break;
                case 16: handleAction(Action::Right, running); break;
            }
        }
    }
}

int App::run() {
    bool running = true;
    while (running) {
        processEvents(running);
        processDownloadCompletion();
        if (!toast_.empty() && SDL_TICKS_PASSED(SDL_GetTicks(), toastUntil_)) toast_.clear();

        if (screen_ == Screen::Browse) ui_.drawBrowse(catalog_, visible_, selectedVisible_, filter_, status_);
        else if (!visible_.empty()) ui_.drawDetail(catalog_.items[visible_[selectedVisible_]], selectedPackage_, status_);
        const DownloadSnapshot snapshot = downloads_.snapshot();
        if (snapshot.state == DownloadState::Running || snapshot.state == DownloadState::Verifying) ui_.drawDownload(snapshot);
        ui_.drawToast(toast_);
        SDL_UpdateWindowSurface(window_);
        SDL_Delay(16);
    }
    return 0;
}

}  // namespace psforcer
