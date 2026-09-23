/**
 * @file main.c
 * @brief segmented_soc example: three-zone SOC arch driven 0 -> 100 -> 0.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 *
 * Demonstrates the fixed-capacity value-domain zones of lv_path_gauge on a
 * dark instrument background: [0, 20) #FC0101, [20, 40) #ED6C00 and
 * [40, 100] #0DD462 over a #525051 track. Two front-ends share one gauge and
 * one animation (same contract as the basic_progress example):
 *
 * - headless (default): a memory display renders into a staging frame, so the
 *   example runs in CI and on machines without a window system; frames at
 *   0/20/40/100 are written as PPM files.
 * - SDL2 window (build with -DLV_PATH_GAUGE_SDL=ON, run with --window): a real
 *   window shows the animation on PC, optionally for a bounded number of
 *   frames (--frames N) so it can be scripted/verified headlessly with
 *   SDL_VIDEODRIVER=dummy.
 *
 * The value animation uses an application-side lv_timer: the gauge itself only
 * stores the value (no animation API), and set_value() stays clamp/store/
 * invalidate.
 *
 * Usage: segmented_soc [--window] [--frames N] [--smoke] [--output <dir>]
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

#define TRACK_COLOR 0x525051
#define GAP_COLOR 0xE8E8E8 /* LV_PART_INDICATOR base colour (gap fallback) */
#define BG_COLOR 0x101418

/* Non-circular SOC arch: one open cubic across the dark screen. */
static const pg_cmd_t g_soc_cmds[] = {
    PG_MOVE_TO(90.0f, 400.0f),
    PG_CUBIC_TO(90.0f, 80.0f, 710.0f, 80.0f, 710.0f, 400.0f),
};
static const pg_path_t g_soc_path = { g_soc_cmds, PG_ARRAY_SIZE(g_soc_cmds) };

static uint16_t g_draw_buf[DRAW_LINES * SCREEN_W];
static uint16_t g_frame[SCREEN_W * SCREEN_H];

static lv_obj_t *g_gauge;
static pg_measure_sample_t g_samples[LV_PATH_GAUGE_MAX_SAMPLES];
static pg_point_t g_vertices[LV_PATH_GAUGE_MAX_VERTICES];
static float g_distances[LV_PATH_GAUGE_MAX_VERTICES];
static lv_path_gauge_workspace_t g_workspace;
static lv_path_gauge_zone_t g_zones[3];
static bool g_memory_display;

struct anim_state {
    int32_t value;   /**< Current value. */
    int32_t step;    /**< Value delta per tick. */
    unsigned ticks;  /**< Ticks served. */
    int reversals;   /**< Times the sweep turned around. */
};

/* --- headless memory display ---------------------------------------------- */

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

static bool display_init_headless(void)
{
    lv_display_t *disp = lv_display_create(SCREEN_W, SCREEN_H);

    if (disp == NULL) {
        return false;
    }
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_buffers(disp, g_draw_buf, NULL, sizeof(g_draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    g_memory_display = true;
    return true;
}

static bool display_init_window(void)
{
#if defined(LV_PATH_GAUGE_SDL) && LV_PATH_GAUGE_SDL
    lv_display_t *disp = lv_sdl_window_create(SCREEN_W, SCREEN_H);

    if (disp == NULL) {
        return false;
    }
    lv_sdl_window_set_title(disp, "lv_path_gauge  segmented_soc");
    lv_sdl_mouse_create();
    g_memory_display = false;
    return true;
#else
    fprintf(stderr,
            "segmented_soc: --window needs an SDL2 build "
            "(configure with -DLV_PATH_GAUGE_SDL=ON)\n");
    return false;
#endif
}

/* --- rendering ------------------------------------------------------------- */

static void render_frame(void)
{
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(lv_display_get_default());
}

static bool write_ppm(const char *dir, const char *name)
{
    char path[512];
    FILE *file;

    if (!g_memory_display) {
        return true; /* SDL window mode has no staging frame */
    }
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "segmented_soc: cannot write %s\n", path);
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
    printf("segmented_soc: wrote %s\n", path);
    return true;
}

static void anim_timer_cb(lv_timer_t *timer)
{
    struct anim_state *state = lv_timer_get_user_data(timer);
    int32_t before = state->value;

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
    if (state->value != before && (state->value == 0 || state->value == 100)) {
        state->reversals++;
    }
}

/* --- main ------------------------------------------------------------------ */

struct options {
    bool window;               /**< Use the SDL2 window display. */
    bool smoke;                /**< Deterministic headless CI run. */
    bool frames_set;           /**< --frames given. */
    unsigned frames;           /**< Ticks to run before exiting. */
    const char *output_dir;    /**< PPM output directory. */
};

static bool parse_options(int argc, char **argv, struct options *opt)
{
    int i;

    memset(opt, 0, sizeof(*opt));
    opt->output_dir = ".";
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--window") == 0) {
            opt->window = true;
        }
        else if (strcmp(argv[i], "--smoke") == 0) {
            opt->smoke = true;
        }
        else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            opt->frames = (unsigned)strtoul(argv[++i], NULL, 10);
            opt->frames_set = true;
        }
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            opt->output_dir = argv[++i];
        }
        else {
            fprintf(stderr,
                    "usage: %s [--window] [--frames N] [--smoke] "
                    "[--output <dir>]\n",
                    argv[0]);
            return false;
        }
    }
    if (opt->smoke) {
        opt->frames = 100;
        opt->frames_set = true;
    }
    return true;
}

int main(int argc, char **argv)
{
    struct options opt;
    struct anim_state state = { 0, 2, 0, 0 };
    lv_timer_t *timer;

    if (!parse_options(argc, argv, &opt)) {
        return 2;
    }

    lv_init();
    if (opt.window) {
        if (!display_init_window()) {
            return 1;
        }
    }
    else if (!display_init_headless()) {
        fprintf(stderr, "segmented_soc: display creation failed\n");
        return 1;
    }

    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(BG_COLOR),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, LV_PART_MAIN);

    g_gauge = lv_path_gauge_create(lv_screen_active());
    if (g_gauge == NULL) {
        fprintf(stderr, "segmented_soc: gauge create failed\n");
        return 1;
    }
    lv_obj_set_size(g_gauge, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(g_gauge, 0, 0);
    lv_obj_set_style_line_width(g_gauge, 14, LV_PART_MAIN);
    lv_obj_set_style_line_color(g_gauge, lv_color_hex(TRACK_COLOR), LV_PART_MAIN);
    lv_obj_set_style_line_opa(g_gauge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_line_width(g_gauge, 8, LV_PART_INDICATOR);
    lv_obj_set_style_line_color(g_gauge, lv_color_hex(GAP_COLOR),
                                LV_PART_INDICATOR);
    lv_obj_set_style_line_opa(g_gauge, LV_OPA_COVER, LV_PART_INDICATOR);

    if (lv_path_gauge_workspace_init(&g_workspace, g_samples,
                                     LV_PATH_GAUGE_MAX_SAMPLES, g_vertices,
                                     g_distances, LV_PATH_GAUGE_MAX_VERTICES,
                                     0.5f) != PG_OK) {
        fprintf(stderr, "segmented_soc: workspace init failed\n");
        return 1;
    }
    {
        pg_result_t res = lv_path_gauge_set_path(g_gauge, &g_soc_path, &g_workspace);

        if (res != PG_OK) {
            fprintf(stderr, "segmented_soc: set_path failed: %s\n",
                    pg_result_str(res));
            return 1;
        }
    }
    lv_path_gauge_set_range(g_gauge, 0, 100);

    /* Production SOC segmentation: half-open value zones, no overlap. */
    g_zones[0] = (lv_path_gauge_zone_t){ 0, 20, lv_color_hex(0xFC0101) };
    g_zones[1] = (lv_path_gauge_zone_t){ 20, 40, lv_color_hex(0xED6C00) };
    g_zones[2] = (lv_path_gauge_zone_t){ 40, 100, lv_color_hex(0x0DD462) };
    {
        pg_result_t res = lv_path_gauge_set_zones(g_gauge, g_zones,
                                                  (uint16_t)PG_ARRAY_SIZE(g_zones));

        if (res != PG_OK) {
            fprintf(stderr, "segmented_soc: set_zones failed: %s\n",
                    pg_result_str(res));
            return 1;
        }
    }
    lv_path_gauge_set_value(g_gauge, 0);

    printf("segmented_soc: display=%s zones=%u total=%.2fpx workspace=%uB\n",
           g_memory_display ? "memory" : "sdl-window", 3u,
           (double)lv_path_gauge_get_total_distance(g_gauge),
           (unsigned)sizeof(lv_path_gauge_workspace_t));

    /* Snapshot frames at the zone boundaries and at full scale. */
    render_frame();
    write_ppm(opt.output_dir, "segmented_soc_000.ppm");
    lv_path_gauge_set_value(g_gauge, 20);
    render_frame();
    write_ppm(opt.output_dir, "segmented_soc_020.ppm");
    lv_path_gauge_set_value(g_gauge, 40);
    render_frame();
    write_ppm(opt.output_dir, "segmented_soc_040.ppm");
    lv_path_gauge_set_value(g_gauge, 100);
    render_frame();
    write_ppm(opt.output_dir, "segmented_soc_100.ppm");

    if (opt.smoke) {
        /* Deterministic 0 -> 100 -> 0 sweep with a redraw-cost measurement. */
        clock_t t0;
        double secs;
        int frame;

        t0 = clock();
        for (frame = 0; frame < 50; frame++) {
            lv_path_gauge_set_value(g_gauge, frame * 2);
            render_frame();
        }
        secs = (double)(clock() - t0) / (double)CLOCKS_PER_SEC;
        printf("segmented_soc: %d frames in %.3fs (%.2f ms/frame)\n", 50, secs,
               secs * 1000.0 / 50.0);
    }

    /* Animation loop; runs until --frames elapses (window mode keeps the
     * window responsive with a short delay between iterations). */
    if (opt.window) {
        timer = lv_timer_create(anim_timer_cb, 16, &state);
        while (!opt.frames_set || state.ticks < opt.frames) {
            lv_tick_inc(16);
            lv_timer_handler();
            lv_delay_ms(5);
        }
        lv_timer_delete(timer);
    }
    else if (opt.frames_set) {
        timer = lv_timer_create(anim_timer_cb, 16, &state);
        while (state.ticks < opt.frames) {
            lv_tick_inc(16);
            lv_timer_handler();
        }
        lv_timer_delete(timer);
    }
    else {
        timer = lv_timer_create(anim_timer_cb, 16, &state);
        for (;;) {
            lv_tick_inc(16);
            lv_timer_handler();
            lv_delay_ms(5);
        }
    }

    printf("segmented_soc: done value=%d ticks=%u reversals=%d\n", state.value,
           state.ticks, state.reversals);
    if (opt.smoke) {
        /* The CI smoke sweep must complete exactly 0 -> 100 -> 0. */
        return (state.value == 0 && state.reversals == 2) ? 0 : 1;
    }
    return 0;
}
