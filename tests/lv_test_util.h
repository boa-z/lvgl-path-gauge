/**
 * @file lv_test_util.h
 * @brief Memory-display harness for host LVGL tests (no window system).
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 *
 * The staging frame and the colour helpers assume LV_COLOR_DEPTH 16
 * (RGB565), which is what config/lv_conf.h pins for the host builds.
 */
#ifndef LV_TEST_UTIL_H
#define LV_TEST_UTIL_H

#include "test_util.h"

#include <lvgl.h>

#include <stdlib.h>
#include <string.h>

#define LV_TEST_DRAW_LINES 40
#define LV_TEST_DRAW_BUF_PIXELS (800 * LV_TEST_DRAW_LINES)

typedef struct {
    lv_display_t *display;   /**< Memory display. */
    uint16_t *frame;         /**< Staged full-screen frame (RGB565). */
    int32_t width;           /**< Display width. */
    int32_t height;          /**< Display height. */
    unsigned flush_count;    /**< Flush callbacks served. */
    unsigned flush_pixels;   /**< Pixels copied. */
} lv_test_display_t;

static uint16_t *g_draw_buf_pixels;

static void lv_test_flush(lv_display_t *disp, const lv_area_t *area,
                          uint8_t *px_map)
{
    lv_test_display_t *td = (lv_test_display_t *)lv_display_get_user_data(disp);
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    int32_t x;
    int32_t y;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            const uint16_t *src =
                (const uint16_t *)px_map + (size_t)y * (size_t)w + (size_t)x;
            int32_t fx = area->x1 + x;
            int32_t fy = area->y1 + y;

            if (fx >= 0 && fy >= 0 && fx < td->width && fy < td->height) {
                td->frame[(size_t)fy * (size_t)td->width + (size_t)fx] = *src;
                td->flush_pixels++;
            }
        }
    }
    td->flush_count++;
    lv_display_flush_ready(disp);
}

/** Creates the memory display and its staging frame. */
static inline bool lv_test_display_init(lv_test_display_t *td, int32_t width,
                                        int32_t height)
{
    memset(td, 0, sizeof(*td));
    td->width = width;
    td->height = height;
    td->frame =
        (uint16_t *)calloc((size_t)width * (size_t)height, sizeof(uint16_t));
    if (td->frame == NULL) {
        return false;
    }
    if (g_draw_buf_pixels == NULL) {
        g_draw_buf_pixels =
            (uint16_t *)malloc(sizeof(uint16_t) * LV_TEST_DRAW_BUF_PIXELS);
        if (g_draw_buf_pixels == NULL) {
            return false;
        }
    }
    td->display = lv_display_create(width, height);
    if (td->display == NULL) {
        return false;
    }
    lv_display_set_user_data(td->display, td);
    lv_display_set_color_format(td->display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(td->display, lv_test_flush);
    lv_display_set_buffers(td->display, g_draw_buf_pixels, NULL,
                           (uint32_t)(sizeof(uint16_t) * LV_TEST_DRAW_BUF_PIXELS),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    return true;
}

/** Invalidates the screen and renders one frame into the staging buffer. */
static inline void lv_test_render(lv_test_display_t *td, lv_obj_t *screen)
{
    lv_obj_invalidate(screen != NULL ? screen : lv_screen_active());
    lv_refr_now(td->display);
}

static inline int32_t lv_test_abs_i(int32_t v)
{
    return v < 0 ? -v : v;
}

/**
 * Expands a staged RGB565 pixel into LVGL's RGB888 lv_color_t.
 *
 * lv_color_t keeps 8-bit channels in LVGL 9.1 (lv_color_hex stores RGB888),
 * while the display buffer is RGB565, so both sides must be normalised before
 * comparison. Bit replication keeps the full 0..255 range.
 */
static inline lv_color_t lv_test_unpack_rgb565(uint16_t px)
{
    uint32_t r5 = (px >> 11) & 0x1Fu;
    uint32_t g6 = (px >> 5) & 0x3Fu;
    uint32_t b5 = px & 0x1Fu;
    lv_color_t out;

    out.red = (uint8_t)((r5 << 3) | (r5 >> 2));
    out.green = (uint8_t)((g6 << 2) | (g6 >> 4));
    out.blue = (uint8_t)((b5 << 3) | (b5 >> 2));
    return out;
}

/** True when the staged pixel matches target within tolerance (RGB888). */
static inline bool lv_test_pixel_is(const lv_test_display_t *td, int32_t x,
                                    int32_t y, lv_color_t target, int32_t tol)
{
    uint16_t px;
    lv_color_t got;

    if (x < 0 || y < 0 || x >= td->width || y >= td->height) {
        return false;
    }
    px = td->frame[(size_t)y * (size_t)td->width + (size_t)x];
    got = lv_test_unpack_rgb565(px);
    return lv_test_abs_i((int32_t)got.red - (int32_t)target.red) <= tol &&
           lv_test_abs_i((int32_t)got.green - (int32_t)target.green) <= tol &&
           lv_test_abs_i((int32_t)got.blue - (int32_t)target.blue) <= tol;
}

/** Counts pixels whose channels are within tolerance of the target colour. */
static inline unsigned lv_test_count_color(const lv_test_display_t *td,
                                           lv_color_t target, int32_t tol)
{
    unsigned count = 0;
    int32_t x;
    int32_t y;

    for (y = 0; y < td->height; y++) {
        for (x = 0; x < td->width; x++) {
            if (lv_test_pixel_is(td, x, y, target, tol)) {
                count++;
            }
        }
    }
    return count;
}

/** Counts target pixels outside the given box (ext-draw-size checks). */
static inline unsigned lv_test_count_color_outside(const lv_test_display_t *td,
                                                   lv_color_t target,
                                                   int32_t tol, int32_t x_max,
                                                   int32_t y_max)
{
    unsigned count = 0;
    int32_t x;
    int32_t y;

    for (y = 0; y < td->height; y++) {
        for (x = 0; x < td->width; x++) {
            if (x <= x_max && y <= y_max) {
                continue;
            }
            if (lv_test_pixel_is(td, x, y, target, tol)) {
                count++;
            }
        }
    }
    return count;
}

#endif /* LV_TEST_UTIL_H */
