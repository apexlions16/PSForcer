#pragma once

// Bu PS4 güvenli yapısında görseller isteğe bağlıdır.
// Açılışın çözücü bağımlılıkları yüzünden durmaması için
// geçici yer tutucu çizimler kullanılır.

#include <SDL2/SDL.h>

#define IMG_INIT_JPG 0x00000001
#define IMG_INIT_PNG 0x00000002

static inline int IMG_Init(int flags) {
    (void)flags;
    return IMG_INIT_PNG | IMG_INIT_JPG;
}

static inline void IMG_Quit(void) {}

static inline const char* IMG_GetError(void) {
    return "Görsel çözücü PS4 güvenli kipinde kapalı";
}

static inline SDL_Texture* IMG_LoadTexture(SDL_Renderer* renderer, const char* path) {
    (void)renderer;
    (void)path;
    return NULL;
}
