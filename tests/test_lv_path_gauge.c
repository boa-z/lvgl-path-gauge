/**
 * @file test_lv_path_gauge.c
 * @brief Gauge state machine: workspace binding, path install, zones API.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "lv_test_util.h"
#include "test_util.h"

#include "lv_path_gauge.h"

#include <limits.h>
#include <string.h>

/* Non-circular open S curve used across the gauge tests. */
static const pg_cmd_t g_s_curve[] = {
    PG_MOVE_TO(60.0f, 400.0f),
    PG_CUBIC_TO(60.0f, 280.0f, 200.0f, 320.0f, 320.0f, 240.0f),
    PG_CUBIC_TO(440.0f, 160.0f, 560.0f, 200.0f, 640.0f, 80.0f),
};
static const pg_path_t g_s_path = { g_s_curve, PG_ARRAY_SIZE(g_s_curve) };

static const pg_cmd_t g_multi_move[] = {
    PG_MOVE_TO(0.0f, 0.0f),
    PG_LINE_TO(10.0f, 0.0f),
    PG_MOVE_TO(20.0f, 0.0f),
    PG_LINE_TO(30.0f, 0.0f),
};
static const pg_path_t g_multi_path = { g_multi_move,
                                        PG_ARRAY_SIZE(g_multi_move) };

static const pg_cmd_t g_closed[] = {
    PG_MOVE_TO(0.0f, 0.0f),
    PG_LINE_TO(10.0f, 0.0f),
    PG_CLOSE(),
};
static const pg_path_t g_closed_path = { g_closed, PG_ARRAY_SIZE(g_closed) };

static const pg_cmd_t g_bad_first[] = {
    PG_LINE_TO(10.0f, 0.0f),
};
static const pg_path_t g_bad_path = { g_bad_first, PG_ARRAY_SIZE(g_bad_first) };

static const pg_cmd_t g_degenerate[] = {
    PG_MOVE_TO(0.0f, 0.0f),
    PG_LINE_TO(0.0f, 0.0f),
};
static const pg_path_t g_degenerate_path = { g_degenerate,
                                             PG_ARRAY_SIZE(g_degenerate) };

/* Tight curvature: exceeds the small workspace capacities below. */
static const pg_cmd_t g_wiggly[] = {
    PG_MOVE_TO(0.0f, 0.0f),
    PG_CUBIC_TO(100.0f, 300.0f, -100.0f, 100.0f, 200.0f, 400.0f),
    PG_CUBIC_TO(500.0f, 700.0f, -300.0f, 300.0f, 400.0f, 800.0f),
};
static const pg_path_t g_wiggly_path = { g_wiggly, PG_ARRAY_SIZE(g_wiggly) };

/* One straight span: fits even the smallest accepted workspace. */
static const pg_cmd_t g_line[] = {
    PG_MOVE_TO(700.0f, 20.0f),
    PG_LINE_TO(780.0f, 20.0f),
};
static const pg_path_t g_line_path = { g_line, PG_ARRAY_SIZE(g_line) };

static pg_measure_sample_t g_samples[LV_PATH_GAUGE_MAX_SAMPLES];
static pg_point_t g_vertices[LV_PATH_GAUGE_MAX_VERTICES];
static float g_distances[LV_PATH_GAUGE_MAX_VERTICES];
static lv_path_gauge_workspace_t g_ws;

static pg_measure_sample_t g_small_samples[8];
static pg_point_t g_small_vertices[8];
static float g_small_distances[8];
static lv_path_gauge_workspace_t g_small_ws;

static pg_measure_sample_t g_vertex_samples[LV_PATH_GAUGE_MAX_SAMPLES];
static lv_path_gauge_workspace_t g_vertex_ws;

static pg_point_t g_vertices_snapshot[LV_PATH_GAUGE_MAX_VERTICES];
static float g_distances_snapshot[LV_PATH_GAUGE_MAX_VERTICES];

/** Counts vertices written by the last set_path(): distances are pre-filled
 *  with -1.0f and written entries are monotonically non-negative. */
static uint16_t count_written_vertices(void)
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
    lv_obj_t *gauge;
    lv_obj_t *gauge_b;
    static lv_path_gauge_zone_t zones[LV_PATH_GAUGE_MAX_ZONES + 1];
    pg_result_t res;
    uint16_t i;
    uint16_t count;

    lv_init();
    TU_EXPECT(lv_is_initialized());
    TU_EXPECT(lv_test_display_init(&td, 800, 480));

    /* Workspace binding: fail-atomic, capacities and tolerance policy. */
    TU_EXPECT(lv_path_gauge_workspace_init(NULL, g_samples,
                                           LV_PATH_GAUGE_MAX_SAMPLES, g_vertices,
                                           g_distances, LV_PATH_GAUGE_MAX_VERTICES,
                                           0.5f) == PG_ERR_INVALID_ARG);
    TU_EXPECT(lv_path_gauge_workspace_init(&g_ws, NULL, LV_PATH_GAUGE_MAX_SAMPLES,
                                           g_vertices, g_distances,
                                           LV_PATH_GAUGE_MAX_VERTICES, 0.5f) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(g_ws.samples == NULL && g_ws.vertices == NULL &&
              g_ws.distances == NULL); /* rejected init zeroes the descriptor */
    TU_EXPECT(lv_path_gauge_workspace_init(&g_ws, g_samples, 1, g_vertices,
                                           g_distances, LV_PATH_GAUGE_MAX_VERTICES,
                                           0.5f) == PG_ERR_WORKSPACE_TOO_SMALL);
    TU_EXPECT(lv_path_gauge_workspace_init(&g_ws, g_samples,
                                           LV_PATH_GAUGE_MAX_SAMPLES, g_vertices,
                                           g_distances, 1, 0.5f) ==
              PG_ERR_WORKSPACE_TOO_SMALL);
    TU_EXPECT(lv_path_gauge_workspace_init(&g_ws, g_samples,
                                           LV_PATH_GAUGE_MAX_SAMPLES, g_vertices,
                                           g_distances, LV_PATH_GAUGE_MAX_VERTICES,
                                           tu_nan()) == PG_ERR_INVALID_ARG);
    TU_EXPECT(lv_path_gauge_workspace_init(&g_ws, g_samples,
                                           LV_PATH_GAUGE_MAX_SAMPLES, g_vertices,
                                           g_distances, LV_PATH_GAUGE_MAX_VERTICES,
                                           tu_inf()) == PG_ERR_INVALID_ARG);
    TU_EXPECT(g_ws.samples == NULL); /* still zeroed after a rejected init */
    res = lv_path_gauge_workspace_init(&g_ws, g_samples, LV_PATH_GAUGE_MAX_SAMPLES,
                                       g_vertices, g_distances,
                                       LV_PATH_GAUGE_MAX_VERTICES, 0.25f);
    TU_EXPECT(res == PG_OK);
    TU_NEAR(g_ws.tolerance, 0.25f, 1e-6f);
    TU_EXPECT(g_ws.samples == g_samples && g_ws.vertices == g_vertices &&
              g_ws.distances == g_distances);
    TU_EXPECT(g_ws.samples_capacity == LV_PATH_GAUGE_MAX_SAMPLES &&
              g_ws.vertices_capacity == LV_PATH_GAUGE_MAX_VERTICES);
    TU_EXPECT(lv_path_gauge_workspace_init(&g_ws, g_samples,
                                           LV_PATH_GAUGE_MAX_SAMPLES, g_vertices,
                                           g_distances, LV_PATH_GAUGE_MAX_VERTICES,
                                           0.0f) == PG_OK);
    TU_NEAR(g_ws.tolerance, LV_PATH_GAUGE_DEFAULT_TOLERANCE, 1e-6f);
    TU_EXPECT(lv_path_gauge_workspace_init(&g_ws, g_samples,
                                           LV_PATH_GAUGE_MAX_SAMPLES, g_vertices,
                                           g_distances, LV_PATH_GAUGE_MAX_VERTICES,
                                           0.25f) == PG_OK);

    /* Creation and defaults. */
    TU_EXPECT(lv_path_gauge_create(NULL) == NULL);
    gauge = lv_path_gauge_create(lv_screen_active());
    TU_EXPECT(gauge != NULL);
    TU_EXPECT(lv_path_gauge_get_min(gauge) == 0);
    TU_EXPECT(lv_path_gauge_get_max(gauge) == 100);
    TU_EXPECT(lv_path_gauge_get_value(gauge) == 0);
    TU_EXPECT(lv_path_gauge_get_total_distance(gauge) == 0.0f);
    TU_NEAR(lv_path_gauge_get_value_fraction(gauge), 0.0f, 1e-6f);

    /* Range guards: min >= max is rejected and keeps the old range. */
    TU_EXPECT(lv_path_gauge_set_range(gauge, 50, 50) == PG_ERR_INVALID_ARG);
    TU_EXPECT(lv_path_gauge_set_range(gauge, 60, 10) == PG_ERR_INVALID_ARG);
    TU_EXPECT(lv_path_gauge_get_min(gauge) == 0);
    TU_EXPECT(lv_path_gauge_get_max(gauge) == 100);
    TU_EXPECT(lv_path_gauge_set_range(gauge, -50, 150) == PG_OK);
    TU_EXPECT(lv_path_gauge_get_min(gauge) == -50);
    TU_EXPECT(lv_path_gauge_get_max(gauge) == 150);
    TU_EXPECT(lv_path_gauge_set_range(NULL, 0, 100) == PG_ERR_INVALID_ARG);

    /* Value clamping and fraction mapping (int32 arithmetic cannot overflow). */
    lv_path_gauge_set_value(gauge, -999);
    TU_EXPECT(lv_path_gauge_get_value(gauge) == -50);
    TU_NEAR(lv_path_gauge_get_value_fraction(gauge), 0.0f, 1e-6f);
    lv_path_gauge_set_value(gauge, 999);
    TU_EXPECT(lv_path_gauge_get_value(gauge) == 150);
    TU_NEAR(lv_path_gauge_get_value_fraction(gauge), 1.0f, 1e-6f);
    lv_path_gauge_set_value(gauge, 50);
    TU_NEAR(lv_path_gauge_get_value_fraction(gauge), 0.5f, 1e-6f);
    TU_EXPECT(lv_path_gauge_set_range(gauge, INT32_MIN + 1, INT32_MAX) == PG_OK);
    lv_path_gauge_set_value(gauge, 0);
    TU_EXPECT(lv_path_gauge_get_value(gauge) == 0);
    {
        float fraction = lv_path_gauge_get_value_fraction(gauge);

        TU_EXPECT(fraction > 0.4999f && fraction < 0.5001f);
    }
    TU_EXPECT(lv_path_gauge_set_range(gauge, 0, 100) == PG_OK);
    lv_path_gauge_set_value(NULL, 50); /* must not crash */

    /* Path installation: one measure + one flatten into caller storage. */
    for (i = 0; i < LV_PATH_GAUGE_MAX_VERTICES; i++) {
        g_distances[i] = -1.0f;
    }
    res = lv_path_gauge_set_path(gauge, &g_s_path, &g_ws);
    TU_EXPECT(res == PG_OK);
    count = count_written_vertices();
    TU_EXPECT(count > 2u);
    TU_EXPECT(g_distances[0] == 0.0f);
    for (i = 1; i < count; i++) {
        TU_EXPECT(g_distances[i] >= g_distances[i - 1u]);
    }
    TU_NEAR(lv_path_gauge_get_total_distance(gauge), g_distances[count - 1u],
            1e-4f);
    TU_POINT_NEAR(g_vertices[0], 60.0f, 400.0f, 1e-3f);
    TU_POINT_NEAR(g_vertices[count - 1u], 640.0f, 80.0f, 0.5f);
    /* The gauge only reads the descriptor: tolerance is untouched. */
    TU_NEAR(g_ws.tolerance, 0.25f, 1e-6f);

    /* set_value() must not touch the caller cache. */
    memcpy(g_vertices_snapshot, g_vertices, sizeof(g_vertices));
    memcpy(g_distances_snapshot, g_distances, sizeof(g_distances));
    lv_path_gauge_set_value(gauge, 25);
    lv_path_gauge_set_value(gauge, 100);
    lv_path_gauge_set_value(gauge, 0);
    TU_EXPECT(memcmp(g_vertices, g_vertices_snapshot, sizeof(g_vertices)) == 0);
    TU_EXPECT(memcmp(g_distances, g_distances_snapshot, sizeof(g_distances)) == 0);

    /* Explicit clear: no NULL-clear semantics on set_path() any more. */
    TU_EXPECT(lv_path_gauge_clear_path(NULL) == PG_ERR_INVALID_ARG);
    TU_EXPECT(lv_path_gauge_clear_path(gauge) == PG_OK);
    TU_EXPECT(lv_path_gauge_get_total_distance(gauge) == 0.0f);
    TU_EXPECT(lv_path_gauge_clear_path(gauge) == PG_OK); /* idempotent */
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_s_path, &g_ws) == PG_OK);
    TU_EXPECT(lv_path_gauge_set_path(gauge, NULL, &g_ws) == PG_ERR_INVALID_ARG);
    TU_EXPECT(lv_path_gauge_get_total_distance(gauge) == 0.0f);

    /* Invalid descriptors are rejected and leave no stale geometry. */
    {
        static lv_path_gauge_workspace_t zero_ws; /* NULL storage */

        TU_EXPECT(lv_path_gauge_set_path(gauge, &g_s_path, &zero_ws) ==
                  PG_ERR_INVALID_ARG);
        TU_EXPECT(lv_path_gauge_get_total_distance(gauge) == 0.0f);
    }
    {
        static lv_path_gauge_workspace_t bad_tolerance_ws;
        static pg_measure_sample_t tol_samples[LV_PATH_GAUGE_MAX_SAMPLES];
        static pg_point_t tol_vertices[LV_PATH_GAUGE_MAX_VERTICES];
        static float tol_distances[LV_PATH_GAUGE_MAX_VERTICES];

        TU_EXPECT(lv_path_gauge_workspace_init(&bad_tolerance_ws, tol_samples,
                                               LV_PATH_GAUGE_MAX_SAMPLES,
                                               tol_vertices, tol_distances,
                                               LV_PATH_GAUGE_MAX_VERTICES,
                                               0.5f) == PG_OK);
        bad_tolerance_ws.tolerance = tu_inf(); /* hand-built descriptor */
        TU_EXPECT(lv_path_gauge_set_path(gauge, &g_s_path, &bad_tolerance_ws) ==
                  PG_ERR_INVALID_ARG);
        TU_EXPECT(lv_path_gauge_get_total_distance(gauge) == 0.0f);
    }

    /* Invalid paths are rejected and never leave stale geometry. */
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_multi_path, &g_ws) ==
              PG_ERR_INVALID_PATH);
    TU_EXPECT(lv_path_gauge_get_total_distance(gauge) == 0.0f);
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_closed_path, &g_ws) ==
              PG_ERR_INVALID_PATH);
    TU_EXPECT(lv_path_gauge_get_total_distance(gauge) == 0.0f);
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_bad_path, &g_ws) ==
              PG_ERR_INVALID_PATH);
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_degenerate_path, &g_ws) ==
              PG_ERR_DEGENERATE);
    TU_EXPECT(lv_path_gauge_get_total_distance(gauge) == 0.0f);

    /* Caller workspace capacity exhaustion: LUT and vertex storage fail
     * explicitly (PG_ERR_WORKSPACE_TOO_SMALL) and never truncate geometry. */
    TU_EXPECT(lv_path_gauge_workspace_init(&g_small_ws, g_small_samples, 4,
                                           g_small_vertices, g_small_distances, 8,
                                           0.05f) == PG_OK);
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_wiggly_path, &g_small_ws) ==
              PG_ERR_WORKSPACE_TOO_SMALL);
    TU_EXPECT(lv_path_gauge_get_total_distance(gauge) == 0.0f);
    /* Baseline: the same path fits a generous workspace at the same tolerance. */
    TU_EXPECT(lv_path_gauge_workspace_init(&g_ws, g_samples,
                                           LV_PATH_GAUGE_MAX_SAMPLES, g_vertices,
                                           g_distances, LV_PATH_GAUGE_MAX_VERTICES,
                                           0.25f) == PG_OK);
    for (i = 0; i < LV_PATH_GAUGE_MAX_VERTICES; i++) {
        g_distances[i] = -1.0f;
    }
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_wiggly_path, &g_ws) == PG_OK);
    count = count_written_vertices();
    TU_EXPECT(count > 8u); /* would not fit the small vertex storage */
    /* Vertex exhaustion: generous LUT, 8 vertices. */
    TU_EXPECT(lv_path_gauge_workspace_init(&g_vertex_ws, g_vertex_samples,
                                           LV_PATH_GAUGE_MAX_SAMPLES,
                                           g_small_vertices, g_small_distances, 8,
                                           0.25f) == PG_OK);
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_wiggly_path, &g_vertex_ws) ==
              PG_ERR_WORKSPACE_TOO_SMALL);
    TU_EXPECT(lv_path_gauge_get_total_distance(gauge) == 0.0f);

    /* Two gauges with different workspace capacities coexist independently. */
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_s_path, &g_ws) == PG_OK);
    {
        float big_total = lv_path_gauge_get_total_distance(gauge);

        TU_EXPECT(big_total > 0.0f);
        TU_EXPECT(lv_path_gauge_workspace_init(&g_small_ws, g_small_samples, 8,
                                               g_small_vertices, g_small_distances,
                                               8, 0.5f) == PG_OK);
        gauge_b = lv_path_gauge_create(lv_screen_active());
        TU_EXPECT(gauge_b != NULL);
        lv_obj_set_size(gauge_b, 800, 480);
        lv_obj_set_pos(gauge_b, 0, 0);
        TU_EXPECT(lv_path_gauge_set_path(gauge_b, &g_line_path, &g_small_ws) ==
                  PG_OK);
        TU_NEAR(lv_path_gauge_get_total_distance(gauge_b), 80.0f, 0.5f);
        /* The big S curve does not fit the small workspace ... */
        TU_EXPECT(lv_path_gauge_set_path(gauge_b, &g_s_path, &g_small_ws) ==
                  PG_ERR_WORKSPACE_TOO_SMALL);
        TU_EXPECT(lv_path_gauge_get_total_distance(gauge_b) == 0.0f);
        /* ... and the small gauge recovers while the big one is untouched. */
        TU_EXPECT(lv_path_gauge_set_path(gauge_b, &g_line_path, &g_small_ws) ==
                  PG_OK);
        TU_NEAR(lv_path_gauge_get_total_distance(gauge), big_total, 1e-4f);

        /* Both draw their own path with their own storage. */
        lv_obj_set_style_line_opa(gauge, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_line_opa(gauge_b, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_line_opa(gauge, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_line_opa(gauge_b, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_line_color(gauge, lv_color_hex(0xFF0000),
                                    LV_PART_INDICATOR);
        lv_obj_set_style_line_color(gauge_b, lv_color_hex(0x00FF00),
                                    LV_PART_INDICATOR);
        lv_path_gauge_set_value(gauge, 100);
        lv_path_gauge_set_value(gauge_b, 100);
        lv_test_render(&td, lv_screen_active());
        TU_EXPECT(lv_test_count_color(&td, lv_color_hex(0xFF0000), 8) > 0u);
        TU_EXPECT(lv_test_count_color(&td, lv_color_hex(0x00FF00), 8) > 0u);
    }

    /* Zone API: half-open value domains, ascending and non-overlapping. */
    zones[0] = (lv_path_gauge_zone_t){ 0, 20, lv_color_hex(0xFC0101) };
    zones[1] = (lv_path_gauge_zone_t){ 20, 100, lv_color_hex(0x0DD462) };
    TU_EXPECT(lv_path_gauge_set_zones(gauge, zones, 2) == PG_OK);
    zones[0] = (lv_path_gauge_zone_t){ 20, 20, lv_color_hex(0xFC0101) };
    TU_EXPECT(lv_path_gauge_set_zones(gauge, zones, 1) == PG_ERR_INVALID_ARG);
    zones[0] = (lv_path_gauge_zone_t){ 30, 10, lv_color_hex(0xFC0101) };
    TU_EXPECT(lv_path_gauge_set_zones(gauge, zones, 1) == PG_ERR_INVALID_ARG);
    zones[0] = (lv_path_gauge_zone_t){ 0, 20, lv_color_hex(0xFC0101) };
    zones[1] = (lv_path_gauge_zone_t){ 10, 30, lv_color_hex(0x0DD462) };
    TU_EXPECT(lv_path_gauge_set_zones(gauge, zones, 2) == PG_ERR_INVALID_ARG);
    zones[0] = (lv_path_gauge_zone_t){ 40, 50, lv_color_hex(0xFC0101) };
    zones[1] = (lv_path_gauge_zone_t){ 0, 10, lv_color_hex(0x0DD462) };
    TU_EXPECT(lv_path_gauge_set_zones(gauge, zones, 2) == PG_ERR_INVALID_ARG);
    for (i = 0; i <= LV_PATH_GAUGE_MAX_ZONES; i++) {
        zones[i] = (lv_path_gauge_zone_t){ (int32_t)i * 10,
                                           (int32_t)i * 10 + 5,
                                           lv_color_hex(0x0DD462) };
    }
    TU_EXPECT(lv_path_gauge_set_zones(gauge, zones, LV_PATH_GAUGE_MAX_ZONES + 1) ==
              PG_ERR_WORKSPACE_TOO_SMALL);
    TU_EXPECT(lv_path_gauge_set_zones(gauge, NULL, 1) == PG_ERR_INVALID_ARG);
    TU_EXPECT(lv_path_gauge_set_zones(NULL, zones, 1) == PG_ERR_INVALID_ARG);
    TU_EXPECT(lv_path_gauge_clear_zones(NULL) == PG_ERR_INVALID_ARG);
    zones[0] = (lv_path_gauge_zone_t){ 0, 20, lv_color_hex(0xFC0101) };
    zones[1] = (lv_path_gauge_zone_t){ 20, 100, lv_color_hex(0x0DD462) };
    TU_EXPECT(lv_path_gauge_set_zones(gauge, zones, 2) == PG_OK);
    TU_EXPECT(lv_path_gauge_set_zones(gauge, NULL, 0) == PG_OK); /* count 0 clears */
    TU_EXPECT(lv_path_gauge_set_zones(gauge, zones, 2) == PG_OK);
    TU_EXPECT(lv_path_gauge_clear_zones(gauge) == PG_OK);

    /* Argument validation for the removed-introspection API surface. */
    TU_EXPECT(lv_path_gauge_get_total_distance(NULL) == 0.0f);
    TU_EXPECT(lv_path_gauge_get_value(NULL) == 0);
    TU_EXPECT(lv_path_gauge_get_min(NULL) == 0);
    TU_EXPECT(lv_path_gauge_get_max(NULL) == 0);
    TU_NEAR(lv_path_gauge_get_value_fraction(NULL), 0.0f, 1e-6f);

    free(td.frame);
    return TU_SUMMARY() ? 1 : 0;
}
