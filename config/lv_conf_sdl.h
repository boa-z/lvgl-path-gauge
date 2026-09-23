/**
 * @file lv_conf_sdl.h
 * @brief Host LVGL configuration with the SDL2 window driver enabled.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 *
 * Used by the example programs when this repository is configured with
 * -DLV_PATH_GAUGE_SDL=ON, so the animation can be watched in a real window on
 * PC (same widget code as the headless builds). Software rendering only:
 * LV_USE_DRAW_SDL stays disabled, therefore no SDL_image dependency.
 *
 * Install SDL2 first, e.g. in an MSYS2 UCRT64 shell:
 *   pacman -S mingw-w64-ucrt-x86_64-SDL2
 */
#ifndef LV_CONF_SDL_H
#define LV_CONF_SDL_H

/* Base headless configuration, then the SDL2 driver on top. */
#include "lv_conf.h"

#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
#define LV_SDL_RENDER_MODE LV_DISPLAY_RENDER_MODE_DIRECT
#define LV_SDL_BUF_COUNT 1
#define LV_SDL_ACCELERATED 1
#define LV_SDL_FULLSCREEN 0
#define LV_SDL_DIRECT_EXIT 1
#define LV_SDL_MOUSEWHEEL_MODE LV_SDL_MOUSEWHEEL_MODE_ENCODER

#endif /* LV_CONF_SDL_H */
