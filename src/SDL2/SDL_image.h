#pragma once

// PSForcer keeps artwork optional in this PS4-safe build. OpenOrbis SDL_image
// can pull PNG/JPEG decoder imports into the executable even when the bundled
// catalog contains no artwork, causing startup failure before the first frame.
// These stubs preserve the placeholder UI without linking SDL_image.

#include <SDL2/SDL.h>

#define IMG_INIT_JPG 0x00000001
#define IMG_INIT_PNG 0x00000002

static inline int IMG_Init(int flags) {
    (void)flags;
    return IMG_INIT_PNG | IMG_INIT_JPG;
}

static inline void IMG_Quit(void) {}

static inline const char* IMG_GetError(void) {
    return "SDL_image disabled in PS4 safe mode";
}

static inline SDL_Texture* IMG_LoadTexture(SDL_Renderer* renderer, const char* path) {
    (void)renderer;
    (void)path;
    return NULL;
}
