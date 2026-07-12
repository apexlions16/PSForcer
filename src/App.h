#pragma once

#include "AppTypes.h"
#include "DownloadManager.h"
#include "Installer.h"
#include "Ui.h"

#include <SDL2/SDL.h>
#include <memory>
#include <string>
#include <vector>

namespace psforcer {

class App {
public:
    App();
    ~App();
    bool initialize(std::string& error);
    int run();
    void shutdown();
private:
    enum class Screen { Browse, Detail };
    enum class Action { Up, Down, Left, Right, Confirm, Back, Download, Refresh, FilterPrevious, FilterNext, Exit };
    void processEvents(bool& running);
    void handleAction(Action action, bool& running);
    void rebuildVisible();
    void moveBrowse(int horizontal, int vertical);
    void movePackage(int delta);
    bool loadCatalog(std::string& error);
    void startPackageDownload();
    void refreshCatalog();
    void processDownloadCompletion();
    void setToast(const std::string& message, uint32_t durationMs = 4500);
    std::string runtimeRoot() const;
    std::string bundledPath(const std::string& relative) const;
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_Joystick* controller_;
    Ui ui_;
    CatalogData catalog_;
    std::vector<size_t> visible_;
    Screen screen_;
    int filter_;
    size_t selectedVisible_;
    size_t selectedPackage_;
    std::string status_;
    std::string toast_;
    uint32_t toastUntil_;
    DownloadManager downloads_;
    std::unique_ptr<Installer> installer_;
    uint64_t nextJobId_;
    uint64_t lastHandledJobId_;
    bool pendingCatalog_;
    size_t pendingItemIndex_;
    size_t pendingPackageIndex_;
    std::string pendingFinalPath_;
};

}  // namespace psforcer
