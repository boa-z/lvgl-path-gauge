/**
 * @file test_lv_path_gauge_draw.c
 * @brief 800x480 memory-display draw smoke test for the gauge.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "lv_test_util.h"
#include "test_util.h"

#include "lv_path_gauge.h"

#include <stdlib.h>

#define TRACK_COLOR lv_color_hex(0xFF0000)
#define PROGRESS_COLOR lv_color_hex(0x00FF00)
/* RGB565 quantisation allows up to 8 levels of channel error. */
#define COLOR_TOL 8

static const pg_cmd_t g_s_curve[] = {
    PG_MOVE_TO(60.0f, 400.0f),
    PG_CUBIC_TO(60.0f, 280.0f, 200.0f, 320.0f, 320.0f, 240.0f),
    PG_CUBIC_TO(440.0f, 160.0f, 560.0f, 200.0f, 640.0f, 80.0f),
};
static const pg_path_t g_s_path = { g_s_curve, PG_ARRAY_SIZE(g_s_curve) };

static pg_measure_sample_t g_samples[LV_PATH_GAUGE_MAX_SAMPLES];
static pg_point_t g_vertices[LV_PATH_GAUGE_MAX_VERTICES];
static float g_distances[LV_PATH_GAUGE_MAX_VERTICES];
static lv_path_gauge_workspace_t g_ws;

static pg_measure_sample_t g_clip_samples[LV_PATH_GAUGE_MAX_SAMPLES];
static pg_point_t g_clip_vertices[LV_PATH_GAUGE_MAX_VERTICES];
static float g_clip_distances[LV_PATH_GAUGE_MAX_VERTICES];
static lv_path_gauge_workspace_t g_clip_ws;

/** Renders at the given value and counts track/progress pixels. */
static void render_and_count(lv_test_display_t *td, lv_obj_t *gauge, int32_t value,
                             unsigned *track, unsigned *progress)
{
    lv_path_gauge_set_value(gauge, value);
    lv_test_render(td, lv_screen_active());
    *track = lv_test_count_color(td, TRACK_COLOR, COLOR_TOL);
    *progress = lv_test_count_color(td, PROGRESS_COLOR, COLOR_TOL);
}

int main(void)
{
    lv_test_display_t td;
    lv_obj_t *screen;
    lv_obj_t *gauge;
    unsigned track0;
    unsigned track50;
    unsigned track100;
    unsigned progress0;
    unsigned progress25;
    unsigned progress50;
    unsigned progress100;

    lv_init();
    TU_EXPECT(lv_is_initialized());
    TU_EXPECT(lv_test_display_init(&td, 800, 480));
    screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    gauge = lv_path_gauge_create(screen);
    TU_EXPECT(gauge != NULL);
    lv_obj_set_size(gauge, 800, 480);
    lv_obj_set_pos(gauge, 0, 0);
    lv_obj_set_style_line_width(gauge, 10, LV_PART_MAIN);
    lv_obj_set_style_line_color(gauge, TRACK_COLOR, LV_PART_MAIN);
    lv_obj_set_style_line_opa(gauge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_line_width(gauge, 4, LV_PART_INDICATOR);
    lv_obj_set_style_line_color(gauge, PROGRESS_COLOR, LV_PART_INDICATOR);
    lv_obj_set_style_line_opa(gauge, LV_OPA_COVER, LV_PART_INDICATOR);

    TU_EXPECT(lv_path_gauge_workspace_init(&g_ws, g_samples,
                                           LV_PATH_GAUGE_MAX_SAMPLES, g_vertices,
                                           g_distances, LV_PATH_GAUGE_MAX_VERTICES,
                                           0.5f) == PG_OK);
    for (uint16_t i = 0; i < LV_PATH_GAUGE_MAX_VERTICES; i++) {
        g_distances[i] = -1.0f; /* sentinel: count the vertices set_path writes */
    }
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_s_path, &g_ws) == PG_OK);
    {
        uint16_t count = 0;

        while (count < LV_PATH_GAUGE_MAX_VERTICES && g_distances[count] >= 0.0f) {
            count++;
        }
        TU_EXPECT(count > 2u);
        TU_EXPECT(g_distances[count - 1u] > 100.0f);
    }

    /* 0%: track only. */
    render_and_count(&td, gauge, 0, &track0, &progress0);
    /* 25 / 50 / 100%: progress grows monotonically. */
    render_and_count(&td, gauge, 25, &track0, &progress25);
    render_and_count(&td, gauge, 50, &track50, &progress50);
    render_and_count(&td, gauge, 100, &track100, &progress100);

    TU_EXPECT(td.flush_count > 0u && td.flush_pixels > 0u);
    TU_EXPECT(track0 > 1000u);      /* the whole path is stroked */
    TU_EXPECT(progress0 == 0u);     /* value == min draws no progress */
    TU_EXPECT(progress25 > 0u);
    TU_EXPECT(progress50 > progress25);
    TU_EXPECT(progress100 > progress50);
    TU_EXPECT(track50 > 1000u);     /* track stays visible under the progress */
    TU_EXPECT(track100 > 1000u);
    /* Progress never covers the whole track: the indicator is thinner. */
    TU_EXPECT(progress100 < track0);
    /* 50% is roughly half of the drawn progress length. */
    TU_EXPECT(progress50 > progress100 / 3u);
    TU_EXPECT(progress50 < (progress100 * 2u) / 3u);

    printf("draw: track(0%%)=%u progress0=%u progress25=%u progress50=%u "
           "progress100=%u flushes=%u\n",
           track0, progress0, progress25, progress50, progress100,
           td.flush_count);

    /* Ext draw size: a thick stroke on a path larger than the object must not
     * be clipped by the object's invalid area. */
    {
        lv_obj_t *clipped = lv_path_gauge_create(screen);
        unsigned outside;

        TU_EXPECT(clipped != NULL);
        lv_obj_set_size(clipped, 200, 200); /* far smaller than the path bbox */
        lv_obj_set_pos(clipped, 0, 0);
        lv_obj_set_style_line_width(clipped, 12, LV_PART_MAIN);
        lv_obj_set_style_line_color(clipped, TRACK_COLOR, LV_PART_MAIN);
        lv_obj_set_style_line_opa(clipped, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_line_width(clipped, 0, LV_PART_INDICATOR);
        TU_EXPECT(lv_path_gauge_workspace_init(&g_clip_ws, g_clip_samples,
                                               LV_PATH_GAUGE_MAX_SAMPLES,
                                               g_clip_vertices, g_clip_distances,
                                               LV_PATH_GAUGE_MAX_VERTICES,
                                               0.5f) == PG_OK);
        TU_EXPECT(lv_path_gauge_set_path(clipped, &g_s_path, &g_clip_ws) == PG_OK);
        lv_path_gauge_set_value(clipped, 0);
        lv_test_render(&td, screen);
        outside = lv_test_count_color_outside(&td, TRACK_COLOR, COLOR_TOL, 210,
                                              210);
        printf("draw: outside-object track pixels=%u\n", outside);
        TU_EXPECT(outside > 0u);
    }

    free(td.frame);
    return TU_SUMMARY() ? 1 : 0;
}
