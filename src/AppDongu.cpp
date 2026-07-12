#include "App.h"

namespace psforcer {

void App::handleAction(Action action, bool& running) {
    if (action == Action::Exit) {
        running = false;
        return;
    }
    if (action == Action::Refresh) {
        refreshCatalog(false);
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
        mediaCache_.update();
        processCatalogCompletion();
        processDownloadCompletion();
        if (!toast_.empty() && SDL_TICKS_PASSED(SDL_GetTicks(), toastUntil_)) toast_.clear();
        if (screen_ == Screen::Browse)
            ui_.drawBrowse(catalog_, visible_, selectedVisible_, filter_, status_);
        else if (!visible_.empty())
            ui_.drawDetail(catalog_.items[visible_[selectedVisible_]], selectedPackage_, status_);
        const DownloadSnapshot snapshot = downloads_.snapshot();
        if (snapshot.state == DownloadState::Running || snapshot.state == DownloadState::Verifying)
            ui_.drawDownload(snapshot);
        ui_.drawToast(toast_);
        SDL_UpdateWindowSurface(window_);
        SDL_Delay(16);
    }
    return 0;
}

}  // namespace psforcer
