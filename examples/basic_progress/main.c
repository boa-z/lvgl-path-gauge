/**
 * @file main.c
 * @brief basic_progress example: non-circular S curve driven 0 -> 100 -> 0.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 *
 * Runs headless against a memory display, so it works in CI and on machines
 * without a window system. Frames at 0%, 50% and 100% are written as PPM
 * files for visual inspection. The value animation uses an application-side
 * lv_timer: the gauge itself only stores the value (no animation API).
 *
 * Usage: basic_progress [--smoke] [--output <dir>]
 */
#include "lv_path_gauge.h"

#include <lvgl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SCREEN_W 800
#define SCREEN_H 480
#define DRAW_LINES 40

/* Non-circular open contour: two cubics forming an S across the screen. */
static const pg_cmd_t g_path_cmds[] = {
    PG_MOVE_TO(60.0f, 400.0f),
    PG_CUBIC_TO(60.0f, 280.0f, 200.0f, 320.0f, 320.0f, 240.0f),
    PG_CUBIC_TO(440.0f, 160.0f, 560.0f, 200.0f, 640.0f, 80.0f),
};
static const pg_path_t g_path = { g_path_cmds, PG_ARRAY_SIZE(g_path_cmds) };

static uint16_t g_draw_buf[DRAW_LINES * SCREEN_W];
static uint16_t g_frame[SCREEN_W * SCREEN_H];

static lv_obj_t *g_gauge;
static lv_path_gauge_workspace_t g_workspace;

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    int32_t x;
    int32_t y;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int32_t fx = area->x1 + x;
            int32_t fy = area->y1 + y;

            if (fx >= 0 && fy >= 0 && fx < SCREEN_W && fy < SCREEN_H) {
                g_frame[(size_t)fy * SCREEN_W + (size_t)fx] =
                    ((const uint16_t *)px_map)[(size_t)y * (size_t)w + (size_t)x];
            }
        }
    }
    lv_display_flush_ready(disp);
}

static void render_frame(void)
{
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(lv_display_get_default());
}

static bool write_ppm(const char *dir, const char *name)
{
    char path[512];
    FILE *file;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "basic_progress: cannot write %s\n", path);
        return false;
    }
    fprintf(file, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
    for (size_t i = 0; i < (size_t)SCREEN_W * SCREEN_H; i++) {
        uint16_t px = g_frame[i];
        unsigned char rgb[3];

        rgb[0] = (unsigned char)(((px >> 11) & 0x1F) * 255 / 31);
        rgb[1] = (unsigned char)(((px >> 5) & 0x3F) * 255 / 63);
        rgb[2] = (unsigned char)((px & 0x1F) * 255 / 31);
        fwrite(rgb, 1, sizeof(rgb), file);
    }
    fclose(file);
    printf("basic_progress: wrote %s\n", path);
    return true;
}

struct anim_state {
    int32_t value;   /**< Current value. */
    int32_t step;    /**< Value delta per tick. */
    unsigned ticks;  /**< Ticks served. */
};

static void anim_timer_cb(lv_timer_t *timer)
{
    struct anim_state *state = lv_timer_get_user_data(timer);

    state->value += state->step;
    if (state->value >= 100) {
        state->value = 100;
        state->step = -2;
    }
    else if (state->value <= 0) {
        state->value = 0;
        state->step = 2;
    }
    /* The gauge only clamps/stores/invalidates; no geometry work here. */
    lv_path_gauge_set_value(g_gauge, state->value);
    state->ticks++;
}

int main(int argc, char **argv)
{
    const char *output_dir = ".";
    bool smoke = false;
    struct anim_state state = { 0, 2, 0 };
    lv_display_t *disp;
    lv_timer_t *timer;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--smoke") == 0) {
            smoke = true;
        }
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_dir = argv[++i];
        }
        else {
            fprintf(stderr, "usage: %s [--smoke] [--output <dir>]\n", argv[0]);
            return 2;
        }
    }

    lv_init();
    disp = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_buffers(disp, g_draw_buf, NULL, sizeof(g_draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x101820),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, LV_PART_MAIN);

    g_gauge = lv_path_gauge_create(lv_screen_active());
    if (g_gauge == NULL) {
        fprintf(stderr, "basic_progress: gauge create failed\n");
        return 1;
    }
    lv_obj_set_size(g_gauge, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(g_gauge, 0, 0);
    lv_obj_set_style_line_width(g_gauge, 12, LV_PART_MAIN);
    lv_obj_set_style_line_color(g_gauge, lv_color_hex(0x5A6470), LV_PART_MAIN);
    lv_obj_set_style_line_opa(g_gauge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_line_width(g_gauge, 6, LV_PART_INDICATOR);
    lv_obj_set_style_line_color(g_gauge, lv_color_hex(0x00B4FF),
                                LV_PART_INDICATOR);
    lv_obj_set_style_line_opa(g_gauge, LV_OPA_COVER, LV_PART_INDICATOR);

    lv_path_gauge_workspace_init(&g_workspace, 0.5f);
    {
        pg_result_t res = lv_path_gauge_set_path(g_gauge, &g_path, &g_workspace);

        if (res != PG_OK) {
            fprintf(stderr, "basic_progress: set_path failed: %s\n",
                    pg_result_str(res));
            return 1;
        }
    }
    lv_path_gauge_set_range(g_gauge, 0, 100);
    lv_path_gauge_set_value(g_gauge, 0);

    printf("basic_progress: vertices=%u total=%.2fpx measure=%.2fpx "
           "workspace=%uB\n",
           lv_path_gauge_get_vertex_count(g_gauge),
           (double)lv_path_gauge_get_total_distance(g_gauge),
           (double)g_workspace.measure.total_length,
           (unsigned)sizeof(lv_path_gauge_workspace_t));

    /* Snapshot frames at 0 / 50 / 100 %. */
    render_frame();
    write_ppm(output_dir, "basic_progress_000.ppm");
    lv_path_gauge_set_value(g_gauge, 50);
    render_frame();
    write_ppm(output_dir, "basic_progress_050.ppm");
    lv_path_gauge_set_value(g_gauge, 100);
    render_frame();
    write_ppm(output_dir, "basic_progress_100.ppm");

    if (smoke) {
        /* Deterministic 0 -> 100 -> 0 animation, then exit. */
        int reversals = 0;
        clock_t t0;
        double secs;
        int frame;

        /* Redraw cost of the cached-geometry path (no measure/flatten). */
        t0 = clock();
        for (frame = 0; frame < 50; frame++) {
            lv_path_gauge_set_value(g_gauge, frame * 2);
            render_frame();
        }
        secs = (double)(clock() - t0) / (double)CLOCKS_PER_SEC;
        printf("basic_progress: %d frames in %.3fs (%.2f ms/frame)\n", 50,
               secs, secs * 1000.0 / 50.0);

        timer = lv_timer_create(anim_timer_cb, 16, &state);
        while (state.ticks < 100) {
            int32_t before = state.value;

            lv_tick_inc(16);
            lv_timer_handler();
            if (state.value != before &&
                (state.value == 0 || state.value == 100)) {
                reversals++;
            }
        }
        lv_timer_delete(timer);
        printf("basic_progress: smoke done, value=%d ticks=%u reversals=%d\n",
               state.value, state.ticks, reversals);
        return (state.value == 0 && reversals == 2) ? 0 : 1;
    }

    timer = lv_timer_create(anim_timer_cb, 16, &state);
    for (;;) {
        lv_tick_inc(16);
        lv_timer_handler();
    }
    (void)timer;
    return 0;
}
