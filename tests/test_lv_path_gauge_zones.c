/**
 * @file test_lv_path_gauge_zones.c
 * @brief Segmented SOC zones: colours, boundaries, clipping and atomicity.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "lv_test_util.h"
#include "test_util.h"

#include "lv_path_gauge.h"

#include <stdlib.h>

#define ZONE_RED 0xFC0101
#define ZONE_ORANGE 0xED6C00
#define ZONE_GREEN 0x0DD462
#define TRACK_COLOR 0x525051
#define GAP_COLOR 0xE8E8E8 /* LV_PART_INDICATOR base colour (gap fallback) */
#define BG_COLOR 0x101418
/* RGB565 quantisation allows up to 8 levels of channel error. */
#define COLOR_TOL 8

/* Non-circular SOC arch; x(t) is monotone, so one column sees one zone. */
static const pg_cmd_t g_arch[] = {
    PG_MOVE_TO(90.0f, 400.0f),
    PG_CUBIC_TO(90.0f, 80.0f, 710.0f, 80.0f, 710.0f, 400.0f),
};
static const pg_path_t g_arch_path = { g_arch, PG_ARRAY_SIZE(g_arch) };

static pg_measure_sample_t g_samples[LV_PATH_GAUGE_MAX_SAMPLES];
static pg_point_t g_vertices[LV_PATH_GAUGE_MAX_VERTICES];
static float g_distances[LV_PATH_GAUGE_MAX_VERTICES];
static lv_path_gauge_workspace_t g_ws;

/** Dominant zone index in a column, or -1 when no zone colour is present. */
static int dominant_zone(const lv_test_display_t *td, int32_t x,
                         const lv_color_t *colors, int count)
{
    int best = -1;
    unsigned best_n = 0;
    int z;

    for (z = 0; z < count; z++) {
        unsigned n = 0;
        int32_t y;

        for (y = 0; y < td->height; y++) {
            if (lv_test_pixel_is(td, x, y, colors[z], COLOR_TOL)) {
                n++;
            }
        }
        if (n > best_n) {
            best_n = n;
            best = z;
        }
    }
    return best;
}

/**
 * Counts zone transitions across the frame and asserts that the zone index
 * never goes backwards: colours meet in order and never interleave (overlap).
 */
static int zone_transitions(const lv_test_display_t *td, const lv_color_t *colors,
                            int count)
{
    int last = -1;
    int transitions = 0;
    int32_t x;

    for (x = 0; x < td->width; x++) {
        int z = dominant_zone(td, x, colors, count);

        if (z < 0) {
            continue;
        }
        if (last >= 0 && z != last) {
            TU_EXPECT(z > last);
            transitions++;
        }
        last = z;
    }
    return transitions;
}

int main(void)
{
    lv_test_display_t td;
    lv_obj_t *screen;
    lv_obj_t *gauge;
    lv_color_t red = lv_color_hex(ZONE_RED);
    lv_color_t orange = lv_color_hex(ZONE_ORANGE);
    lv_color_t green = lv_color_hex(ZONE_GREEN);
    lv_color_t track = lv_color_hex(TRACK_COLOR);
    lv_color_t gap = lv_color_hex(GAP_COLOR);
    const lv_color_t zone_colors[3] = { red, orange, green };
    static const int32_t values[] = { 0,  1,  19, 20, 21,  39,  40,
                                      41, 50, 53, 75, 99, 100 };
    enum { NVALUES = (int)(sizeof(values) / sizeof(values[0])) };
    unsigned reds[NVALUES];
    unsigned oranges[NVALUES];
    unsigned greens[NVALUES];
    unsigned gaps[NVALUES];
    unsigned tracks[NVALUES];
    static lv_path_gauge_zone_t zones[LV_PATH_GAUGE_MAX_ZONES + 1];
    unsigned red_saved;
    unsigned green_saved;
    int i;

    lv_init();
    TU_EXPECT(lv_is_initialized());
    TU_EXPECT(lv_test_display_init(&td, 800, 480));
    screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(BG_COLOR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    gauge = lv_path_gauge_create(screen);
    TU_EXPECT(gauge != NULL);
    lv_obj_set_size(gauge, 800, 480);
    lv_obj_set_pos(gauge, 0, 0);
    lv_obj_set_style_line_width(gauge, 14, LV_PART_MAIN);
    lv_obj_set_style_line_color(gauge, track, LV_PART_MAIN);
    lv_obj_set_style_line_opa(gauge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_line_width(gauge, 8, LV_PART_INDICATOR);
    lv_obj_set_style_line_color(gauge, gap, LV_PART_INDICATOR);
    lv_obj_set_style_line_opa(gauge, LV_OPA_COVER, LV_PART_INDICATOR);

    TU_EXPECT(lv_path_gauge_workspace_init(&g_ws, g_samples,
                                           LV_PATH_GAUGE_MAX_SAMPLES, g_vertices,
                                           g_distances, LV_PATH_GAUGE_MAX_VERTICES,
                                           0.5f) == PG_OK);
    for (i = 0; i < (int)LV_PATH_GAUGE_MAX_VERTICES; i++) {
        g_distances[i] = -1.0f; /* sentinel: count the vertices set_path writes */
    }
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_arch_path, &g_ws) == PG_OK);
    {
        uint16_t count = 0;

        while (count < LV_PATH_GAUGE_MAX_VERTICES && g_distances[count] >= 0.0f) {
            count++;
        }
        TU_EXPECT(count > 2u);
        TU_EXPECT(g_distances[count - 1u] > 100.0f);
    }

    /* Production SOC segmentation: [0,20) red, [20,40) orange, [40,100] green. */
    zones[0] = (lv_path_gauge_zone_t){ 0, 20, red };
    zones[1] = (lv_path_gauge_zone_t){ 20, 40, orange };
    zones[2] = (lv_path_gauge_zone_t){ 40, 100, green };
    TU_EXPECT(lv_path_gauge_set_zones(gauge, zones, 3) == PG_OK);

    for (i = 0; i < NVALUES; i++) {
        lv_path_gauge_set_value(gauge, values[i]);
        lv_test_render(&td, screen);
        reds[i] = lv_test_count_color(&td, red, COLOR_TOL);
        oranges[i] = lv_test_count_color(&td, orange, COLOR_TOL);
        greens[i] = lv_test_count_color(&td, green, COLOR_TOL);
        gaps[i] = lv_test_count_color(&td, gap, COLOR_TOL);
        tracks[i] = lv_test_count_color(&td, track, COLOR_TOL);
        printf("zones: value=%3d red=%5u orange=%5u green=%5u gap=%u track=%5u\n",
               values[i], reds[i], oranges[i], greens[i], gaps[i], tracks[i]);
    }

    /* 0%: track only; the track stays visible under every progress frame. */
    TU_EXPECT(reds[0] == 0u && oranges[0] == 0u && greens[0] == 0u);
    for (i = 0; i < NVALUES; i++) {
        TU_EXPECT(tracks[i] > 1000u);
    }

    /* Half-open zone semantics at the boundaries: [start, end) excludes end,
     * so a boundary value belongs to the LATER zone -- but at that exact value
     * the later zone's visible length is zero and only the earlier interval is
     * painted. */
    TU_EXPECT(reds[1] > 0u && oranges[1] == 0u && greens[1] == 0u); /* 1% */
    TU_EXPECT(reds[2] > reds[1]);                                   /* 19% */
    TU_EXPECT(oranges[2] == 0u && greens[2] == 0u);
    TU_EXPECT(reds[3] > reds[2] && oranges[3] == 0u);               /* 20% */
    TU_EXPECT(oranges[4] > 0u && greens[4] == 0u);                  /* 21% */
    TU_EXPECT(oranges[5] > oranges[4] && greens[5] == 0u);          /* 39% */
    TU_EXPECT(oranges[6] > oranges[5] && greens[6] == 0u);          /* 40% */
    TU_EXPECT(greens[7] > 0u);                                      /* 41% */
    TU_EXPECT(reds[7] > 0u && oranges[7] > 0u);
    TU_EXPECT(greens[12] > greens[7]);                              /* 100% */

    /* Progress grows monotonically: the total drawn progress never shrinks. */
    for (i = 1; i < NVALUES; i++) {
        unsigned now = reds[i] + oranges[i] + greens[i] + gaps[i];
        unsigned prev = reds[i - 1u] + oranges[i - 1u] + greens[i - 1u] +
                        gaps[i - 1u];

        TU_EXPECT(now >= prev);
    }
    /* A zone grows while the progress is inside it ... */
    TU_EXPECT(reds[2] >= reds[1] && reds[3] >= reds[2]);
    TU_EXPECT(oranges[5] >= oranges[4] && oranges[6] >= oranges[5]);
    TU_EXPECT(greens[8] >= greens[7] && greens[9] >= greens[8]);
    TU_EXPECT(greens[10] >= greens[9] && greens[11] >= greens[10] &&
              greens[12] >= greens[11]);
    /* ... and stays constant once it is fully painted. */
    for (i = 5; i < NVALUES; i++) {
        TU_EXPECT(reds[i] == reds[4]);
    }
    for (i = 8; i < NVALUES; i++) {
        TU_EXPECT(oranges[i] == oranges[7]);
    }
    /* Rounded caps only at the true end of the whole active run: the terminal
     * run carries a cap that disappears when the next zone takes over (the
     * internal boundary is butt-capped, with the geometry joints inside a run
     * filled by same-colour caps instead). */
    TU_EXPECT(reds[4] < reds[3]);
    TU_EXPECT(oranges[7] < oranges[6]);

    /* Full coverage: no gap colour anywhere. */
    for (i = 0; i < NVALUES; i++) {
        TU_EXPECT(gaps[i] == 0u);
    }

    /* At 100%: red -> orange -> green in order with exactly two boundaries. */
    lv_path_gauge_set_value(gauge, 100);
    lv_test_render(&td, screen);
    TU_EXPECT(zone_transitions(&td, zone_colors, 3) == 2);

    /* Gap zones: [0,20) red, [40,100] green; 20..40 uses the base colour. */
    zones[0] = (lv_path_gauge_zone_t){ 0, 20, red };
    zones[1] = (lv_path_gauge_zone_t){ 40, 100, green };
    TU_EXPECT(lv_path_gauge_set_zones(gauge, zones, 2) == PG_OK);
    lv_path_gauge_set_value(gauge, 100);
    lv_test_render(&td, screen);
    TU_EXPECT(lv_test_count_color(&td, red, COLOR_TOL) > 0u);
    TU_EXPECT(lv_test_count_color(&td, green, COLOR_TOL) > 0u);
    TU_EXPECT(lv_test_count_color(&td, orange, COLOR_TOL) == 0u);
    TU_EXPECT(lv_test_count_color(&td, gap, COLOR_TOL) > 0u);
    {
        /* The gap colour sits between red and green along the path. */
        const lv_color_t gap_colors[3] = { red, gap, green };

        TU_EXPECT(zone_transitions(&td, gap_colors, 3) == 2);
    }

    /* Zones beyond the gauge range are clipped at draw time, never rewritten. */
    TU_EXPECT(lv_path_gauge_set_range(gauge, 0, 100) == PG_OK);
    zones[0] = (lv_path_gauge_zone_t){ -50, 10, red };
    zones[1] = (lv_path_gauge_zone_t){ 50, 150, green };
    TU_EXPECT(lv_path_gauge_set_zones(gauge, zones, 2) == PG_OK);
    lv_path_gauge_set_value(gauge, 100);
    lv_test_render(&td, screen);
    {
        unsigned r = lv_test_count_color(&td, red, COLOR_TOL);
        unsigned g = lv_test_count_color(&td, green, COLOR_TOL);

        TU_EXPECT(r > 0u); /* [-50,10) clips to [0,10) */
        TU_EXPECT(g > r);  /* [50,150] clips to [50,100]: longer */
        TU_EXPECT(lv_test_count_color(&td, gap, COLOR_TOL) > 0u);
    }
    /* Narrower range: the green zone no longer intersects it ... */
    TU_EXPECT(lv_path_gauge_set_range(gauge, 0, 50) == PG_OK);
    lv_path_gauge_set_value(gauge, 50);
    lv_test_render(&td, screen);
    TU_EXPECT(lv_test_count_color(&td, red, COLOR_TOL) > 0u);
    TU_EXPECT(lv_test_count_color(&td, green, COLOR_TOL) == 0u);
    TU_EXPECT(lv_test_count_color(&td, gap, COLOR_TOL) > 0u);
    /* ... and returns with the wider range: the stored config is untouched. */
    TU_EXPECT(lv_path_gauge_set_range(gauge, 0, 100) == PG_OK);
    lv_path_gauge_set_value(gauge, 100);
    lv_test_render(&td, screen);
    TU_EXPECT(lv_test_count_color(&td, green, COLOR_TOL) > 0u);

    /* Invalid zone tables are rejected atomically: the frame is unchanged. */
    zones[0] = (lv_path_gauge_zone_t){ 0, 20, red };
    zones[1] = (lv_path_gauge_zone_t){ 20, 100, green };
    TU_EXPECT(lv_path_gauge_set_zones(gauge, zones, 2) == PG_OK);
    lv_path_gauge_set_value(gauge, 100);
    lv_test_render(&td, screen);
    red_saved = lv_test_count_color(&td, red, COLOR_TOL);
    green_saved = lv_test_count_color(&td, green, COLOR_TOL);
    TU_EXPECT(red_saved > 0u && green_saved > 0u);
    /* overlap */
    zones[0] = (lv_path_gauge_zone_t){ 0, 20, red };
    zones[1] = (lv_path_gauge_zone_t){ 10, 30, orange };
    TU_EXPECT(lv_path_gauge_set_zones(gauge, zones, 2) == PG_ERR_INVALID_ARG);
    /* unsorted */
    zones[0] = (lv_path_gauge_zone_t){ 40, 50, red };
    zones[1] = (lv_path_gauge_zone_t){ 0, 10, green };
    TU_EXPECT(lv_path_gauge_set_zones(gauge, zones, 2) == PG_ERR_INVALID_ARG);
    /* empty zone */
    zones[0] = (lv_path_gauge_zone_t){ 30, 30, red };
    TU_EXPECT(lv_path_gauge_set_zones(gauge, zones, 1) == PG_ERR_INVALID_ARG);
    /* capacity */
    {
        uint16_t n;

        for (n = 0; n <= LV_PATH_GAUGE_MAX_ZONES; n++) {
            zones[n] = (lv_path_gauge_zone_t){ (int32_t)n * 10,
                                               (int32_t)n * 10 + 5, green };
        }
        TU_EXPECT(lv_path_gauge_set_zones(gauge, zones,
                                          LV_PATH_GAUGE_MAX_ZONES + 1) ==
                  PG_ERR_WORKSPACE_TOO_SMALL);
    }
    lv_path_gauge_set_value(gauge, 100);
    lv_test_render(&td, screen);
    TU_EXPECT(lv_test_count_color(&td, red, COLOR_TOL) == red_saved);
    TU_EXPECT(lv_test_count_color(&td, green, COLOR_TOL) == green_saved);
    TU_EXPECT(lv_test_count_color(&td, orange, COLOR_TOL) == 0u);

    /* clear_zones() returns to the single-colour progress. */
    TU_EXPECT(lv_path_gauge_clear_zones(gauge) == PG_OK);
    lv_test_render(&td, screen);
    TU_EXPECT(lv_test_count_color(&td, red, COLOR_TOL) == 0u);
    TU_EXPECT(lv_test_count_color(&td, green, COLOR_TOL) == 0u);
    TU_EXPECT(lv_test_count_color(&td, gap, COLOR_TOL) > 0u);

    free(td.frame);
    return TU_SUMMARY() ? 1 : 0;
}
