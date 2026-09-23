/**
 * @file test_lv_path_gauge_joints.c
 * @brief Stroke continuity: no background holes at cached polyline joints.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 *
 * Phase 5.1 regression: the cached polyline is drawn segment by segment, and a
 * butt joint between two segments leaves a wedge-shaped hole on the outside of
 * the bend which shows the background (or the track) through at thick widths.
 * The renderer must fill every geometry joint inside one colour run with a
 * same-colour cap while keeping semantic boundaries (zone edges, true ends)
 * flat.
 */
#include "lv_test_util.h"
#include "test_util.h"

#include "lv_path_gauge.h"

#include <stdlib.h>

#define BG_COLOR lv_color_hex(0x000000)
#define TRACK_COLOR lv_color_hex(0xFFFFFF)
#define PROGRESS_COLOR lv_color_hex(0x00FF00)
#define ZONE_A_COLOR lv_color_hex(0xFF0000)
#define ZONE_B_COLOR lv_color_hex(0x0000FF)
/* RGB565 quantisation allows up to 8 levels of channel error. */
#define COLOR_TOL 8
/* Phase 5.1 requires a 20-30px stroke; the probe stays inside its footprint. */
#define STROKE_WIDTH 24
#define PROBE_RADIUS (STROKE_WIDTH / 2 - 3)

/* High curvature: sharp corners plus a tight cubic (dense flattened joints). */
static const pg_cmd_t g_thick_cmds[] = {
    PG_MOVE_TO(90.0f, 390.0f),
    PG_LINE_TO(170.0f, 110.0f),
    PG_LINE_TO(250.0f, 390.0f),
    PG_LINE_TO(330.0f, 110.0f),
    PG_CUBIC_TO(430.0f, 30.0f, 530.0f, 450.0f, 640.0f, 230.0f),
    PG_LINE_TO(720.0f, 390.0f),
};
static const pg_path_t g_thick_path = { g_thick_cmds,
                                        PG_ARRAY_SIZE(g_thick_cmds) };

/* Gentle SOC arch: the zone-boundary seam check must not involve corners. */
static const pg_cmd_t g_arch[] = {
    PG_MOVE_TO(90.0f, 400.0f),
    PG_CUBIC_TO(90.0f, 80.0f, 710.0f, 80.0f, 710.0f, 400.0f),
};
static const pg_path_t g_arch_path = { g_arch, PG_ARRAY_SIZE(g_arch) };

static pg_measure_sample_t g_samples[LV_PATH_GAUGE_MAX_SAMPLES];
static pg_point_t g_vertices[LV_PATH_GAUGE_MAX_VERTICES];
static float g_distances[LV_PATH_GAUGE_MAX_VERTICES];
static lv_path_gauge_workspace_t g_ws;
static uint16_t g_count; /* vertices written by the last set_path() */

/** Rounded object-local coordinate, matching the renderer's rounding. */
static int32_t joint_coord(float value)
{
    return (int32_t)(value >= 0.0f ? (value + 0.5f) : (value - 0.5f));
}

/** Counts probe-disc pixels that do not match @p color (a hole shows through). */
static unsigned disc_foreign(const lv_test_display_t *td, int32_t cx, int32_t cy,
                             int32_t radius, lv_color_t color)
{
    unsigned bad = 0;
    int32_t x;
    int32_t y;

    for (y = cy - radius; y <= cy + radius; y++) {
        for (x = cx - radius; x <= cx + radius; x++) {
            int32_t dx = x - cx;
            int32_t dy = y - cy;

            if (dx * dx + dy * dy > radius * radius) {
                continue;
            }
            if (!lv_test_pixel_is(td, x, y, color, COLOR_TOL)) {
                bad++;
            }
        }
    }
    return bad;
}

/**
 * A zone-boundary pixel must be one of the two zone colours or a blend of
 * them: green stays ~0 and red+blue stays ~full scale. A background seam
 * breaks the sum, a track (white) seam breaks green.
 */
static unsigned disc_boundary_foreign(const lv_test_display_t *td, int32_t cx,
                                      int32_t cy, int32_t radius)
{
    unsigned bad = 0;
    int32_t x;
    int32_t y;

    for (y = cy - radius; y <= cy + radius; y++) {
        for (x = cx - radius; x <= cx + radius; x++) {
            int32_t dx = x - cx;
            int32_t dy = y - cy;
            lv_color_t c;

            if (dx * dx + dy * dy > radius * radius) {
                continue;
            }
            if (x < 0 || y < 0 || x >= td->width || y >= td->height) {
                continue;
            }
            c = lv_test_unpack_rgb565(
                td->frame[(size_t)y * (size_t)td->width + (size_t)x]);
            if ((int32_t)c.green > COLOR_TOL ||
                (int32_t)c.red + (int32_t)c.blue < 255 - COLOR_TOL) {
                bad++;
            }
        }
    }
    return bad;
}

/** Point on the cached polyline at @p distance (test-side interpolation). */
static void point_at_distance(float distance, pg_point_t *out)
{
    uint16_t i;

    for (i = 0; i + 1u < g_count; i++) {
        float d0 = g_distances[i];
        float d1 = g_distances[i + 1u];

        if (distance <= d1 && d1 > d0) {
            float t = (distance - d0) / (d1 - d0);

            out->x = g_vertices[i].x +
                     (g_vertices[i + 1u].x - g_vertices[i].x) * t;
            out->y = g_vertices[i].y +
                     (g_vertices[i + 1u].y - g_vertices[i].y) * t;
            return;
        }
    }
    *out = g_vertices[g_count - 1u];
}

/** Counts zone-colour transitions across the frame (no colour reversal). */
static int zone_transitions(const lv_test_display_t *td)
{
    int last = -1;
    int transitions = 0;
    int32_t x;

    for (x = 0; x < td->width; x++) {
        const lv_color_t colors[2] = { ZONE_A_COLOR, ZONE_B_COLOR };
        int best = -1;
        unsigned best_n = 0;
        int32_t y;
        int z;

        for (z = 0; z < 2; z++) {
            unsigned n = 0;

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
        if (best < 0) {
            continue;
        }
        if (last >= 0 && best != last) {
            TU_EXPECT(best > last);
            transitions++;
        }
        last = best;
    }
    return transitions;
}

/** Counts the vertices written by the last set_path() (sentinel scan). */
static uint16_t count_written(void)
{
    uint16_t n = 0;

    while (n < LV_PATH_GAUGE_MAX_VERTICES && g_distances[n] >= 0.0f) {
        n++;
    }
    return n;
}

int main(void)
{
    lv_test_display_t td;
    lv_obj_t *screen;
    lv_obj_t *gauge;
    static lv_path_gauge_zone_t zones[2];
    unsigned bad;
    uint16_t i;

    lv_init();
    TU_EXPECT(lv_is_initialized());
    TU_EXPECT(lv_test_display_init(&td, 800, 480));
    screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, BG_COLOR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    gauge = lv_path_gauge_create(screen);
    TU_EXPECT(gauge != NULL);
    lv_obj_set_size(gauge, 800, 480);
    lv_obj_set_pos(gauge, 0, 0);
    lv_obj_set_style_line_width(gauge, STROKE_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_line_color(gauge, TRACK_COLOR, LV_PART_MAIN);
    lv_obj_set_style_line_opa(gauge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_line_width(gauge, STROKE_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_line_color(gauge, PROGRESS_COLOR, LV_PART_INDICATOR);
    lv_obj_set_style_line_opa(gauge, LV_OPA_COVER, LV_PART_INDICATOR);

    TU_EXPECT(lv_path_gauge_workspace_init(&g_ws, g_samples,
                                           LV_PATH_GAUGE_MAX_SAMPLES, g_vertices,
                                           g_distances, LV_PATH_GAUGE_MAX_VERTICES,
                                           0.5f) == PG_OK);
    for (i = 0; i < LV_PATH_GAUGE_MAX_VERTICES; i++) {
        g_distances[i] = -1.0f;
    }
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_thick_path, &g_ws) == PG_OK);
    g_count = count_written();
    TU_EXPECT(g_count > 8u); /* high curvature: many cached joints */

    /* --- track: every interior joint must be hole-free ------------------- */
    lv_path_gauge_set_value(gauge, 0); /* track only */
    lv_test_render(&td, screen);
    for (i = 1; i + 1u < g_count; i++) {
        bad = disc_foreign(&td, joint_coord(g_vertices[i].x),
                           joint_coord(g_vertices[i].y), PROBE_RADIUS,
                           TRACK_COLOR);
        if (bad > 0u) {
            printf("joints: track hole at vertex %u (%d,%d): %u px\n", i,
                   joint_coord(g_vertices[i].x), joint_coord(g_vertices[i].y),
                   bad);
        }
        TU_EXPECT(bad == 0u);
    }

    /* --- progress: the same joints under a thick indicator --------------- */
    lv_obj_set_style_line_opa(gauge, LV_OPA_TRANSP, LV_PART_MAIN); /* track off */
    lv_path_gauge_set_value(gauge, 100);
    lv_test_render(&td, screen);
    for (i = 1; i + 1u < g_count; i++) {
        bad = disc_foreign(&td, joint_coord(g_vertices[i].x),
                           joint_coord(g_vertices[i].y), PROBE_RADIUS,
                           PROGRESS_COLOR);
        if (bad > 0u) {
            printf("joints: progress hole at vertex %u (%d,%d): %u px\n", i,
                   joint_coord(g_vertices[i].x), joint_coord(g_vertices[i].y),
                   bad);
        }
        TU_EXPECT(bad == 0u);
    }

    /* --- zone boundary: flat, seam-free, no colour reversal --------------- */
    for (i = 0; i < LV_PATH_GAUGE_MAX_VERTICES; i++) {
        g_distances[i] = -1.0f;
    }
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_arch_path, &g_ws) == PG_OK);
    g_count = count_written();
    TU_EXPECT(g_count > 2u);
    zones[0] = (lv_path_gauge_zone_t){ 0, 50, ZONE_A_COLOR };
    zones[1] = (lv_path_gauge_zone_t){ 50, 100, ZONE_B_COLOR };
    TU_EXPECT(lv_path_gauge_set_zones(gauge, zones, 2) == PG_OK);
    lv_path_gauge_set_value(gauge, 100);
    lv_test_render(&td, screen);
    {
        pg_point_t boundary;

        point_at_distance(g_distances[g_count - 1u] * 0.5f, &boundary);
        bad = disc_boundary_foreign(&td, joint_coord(boundary.x),
                                    joint_coord(boundary.y), PROBE_RADIUS);
        if (bad > 0u) {
            printf("joints: zone boundary seam at (%d,%d): %u px\n",
                   joint_coord(boundary.x), joint_coord(boundary.y), bad);
        }
        TU_EXPECT(bad == 0u);
    }
    TU_EXPECT(zone_transitions(&td) == 1);

    free(td.frame);
    return TU_SUMMARY() ? 1 : 0;
}
