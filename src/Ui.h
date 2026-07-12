#pragma once

#include "AppTypes.h"
#include "DownloadManager.h"

#include <SDL2/SDL.h>
#include <map>
#include <string>
#include <vector>

namespace psforcer {

class Ui {
public:
    Ui();
    ~Ui();
    bool initialize(SDL_Renderer* renderer, std::string& error);
    void shutdown();
    void drawBrowse(const CatalogData& catalog,
                    const std::vector<size_t>& visible,
                    size_t selected,
                    int filter,
                    const std::string& status);
    void drawDetail(const CatalogItem& item,
                    size_t selectedPackage,
                    const std::string& status);
    void drawDownload(const DownloadSnapshot& snapshot);
    void drawToast(const std::string& message);
private:
    SDL_Texture* texture(const std::string& path);
    void fillRect(int x, int y, int w, int h, const Color& color);
    void strokeRect(int x, int y, int w, int h, const Color& color, int thickness = 2);
    void drawText(const std::string& text, int x, int y, int height, const Color& color, int maxWidth = 0);
    void drawWrapped(const std::string& text, int x, int y, int width, int height, int lineHeight, const Color& color, int maxLines);
    void drawImage(SDL_Texture* image, int x, int y, int w, int h, bool cover);
    void drawBadge(const std::string& text, int x, int y, const Color& color);
    std::string filterName(int filter) const;
    static std::string asciiSafe(const std::string& value);
    SDL_Renderer* renderer_;
    std::map<std::string, SDL_Texture*> textures_;
};

}  // namespace psforcer
