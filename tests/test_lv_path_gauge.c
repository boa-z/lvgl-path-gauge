/**
 * @file test_lv_path_gauge.c
 * @brief Gauge state machine: path install, ranges, clamping, cache reuse.
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

/* Tight tolerance and many leaves: exceeds the default vertex capacity. */
static const pg_cmd_t g_wiggly[] = {
    PG_MOVE_TO(0.0f, 0.0f),
    PG_CUBIC_TO(100.0f, 300.0f, -100.0f, 100.0f, 200.0f, 400.0f),
    PG_CUBIC_TO(500.0f, 700.0f, -300.0f, 300.0f, 400.0f, 800.0f),
};
static const pg_path_t g_wiggly_path = { g_wiggly, PG_ARRAY_SIZE(g_wiggly) };

int main(void)
{
    lv_test_display_t td;
    lv_obj_t *gauge;
    static lv_path_gauge_workspace_t ws;
    static lv_path_gauge_workspace_t ws_small;
    static lv_path_gauge_workspace_t ws_snapshot;
    pg_result_t res;
    uint16_t i;

    lv_init();
    TU_EXPECT(lv_is_initialized());
    TU_EXPECT(lv_test_display_init(&td, 800, 480));

    /* Workspace initialisation. */
    TU_EXPECT(ws.tolerance == 0.0f);
    lv_path_gauge_workspace_init(&ws, 0.25f);
    TU_NEAR(ws.tolerance, 0.25f, 1e-6f);
    TU_EXPECT(ws.vertex_count == 0u && ws.total_distance == 0.0f);
    lv_path_gauge_workspace_init(&ws, 0.0f);
    TU_NEAR(ws.tolerance, LV_PATH_GAUGE_DEFAULT_TOLERANCE, 1e-6f);
    lv_path_gauge_workspace_init(&ws, 0.25f);
    lv_path_gauge_workspace_init(NULL, 0.5f); /* must not crash */

    /* Creation and defaults. */
    TU_EXPECT(lv_path_gauge_create(NULL) == NULL);
    gauge = lv_path_gauge_create(lv_screen_active());
    TU_EXPECT(gauge != NULL);
    TU_EXPECT(lv_path_gauge_get_min(gauge) == 0);
    TU_EXPECT(lv_path_gauge_get_max(gauge) == 100);
    TU_EXPECT(lv_path_gauge_get_value(gauge) == 0);
    TU_EXPECT(lv_path_gauge_get_vertex_count(gauge) == 0u);
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

    /* Path installation: measure + flatten happen exactly once here. */
    res = lv_path_gauge_set_path(gauge, &g_s_path, &ws);
    TU_EXPECT(res == PG_OK);
    TU_EXPECT(lv_path_gauge_get_vertex_count(gauge) > 2u);
    TU_EXPECT(ws.vertex_count == lv_path_gauge_get_vertex_count(gauge));
    TU_EXPECT(ws.total_distance > 0.0f);
    TU_EXPECT(ws.measure.sample_count >= 2u);
    TU_EXPECT(ws.measure.total_length > 0.0f);
    TU_NEAR(ws.measure.total_length / ws.total_distance, 1.0f, 0.01f);
    TU_NEAR(lv_path_gauge_get_vertex_distance(gauge, 0u), 0.0f, 1e-6f);
    TU_NEAR(lv_path_gauge_get_vertex_distance(gauge,
                                              (uint16_t)(ws.vertex_count - 1u)),
            ws.total_distance, 1e-4f);
    for (i = 1; i < ws.vertex_count; i++) {
        TU_EXPECT(lv_path_gauge_get_vertex_distance(gauge, i) >=
                  lv_path_gauge_get_vertex_distance(gauge, (uint16_t)(i - 1u)));
    }
    TU_POINT_NEAR(lv_path_gauge_get_vertex_point(gauge, 0u), 60.0f, 400.0f, 1e-3f);
    TU_POINT_NEAR(lv_path_gauge_get_vertex_point(gauge,
                                                 (uint16_t)(ws.vertex_count - 1u)),
                  640.0f, 80.0f, 0.5f);
    /* Out-of-range diagnostics are safe. */
    TU_POINT_NEAR(lv_path_gauge_get_vertex_point(gauge, 0xFFFFu), 0.0f, 0.0f,
                  1e-6f);
    TU_EXPECT(lv_path_gauge_get_vertex_distance(gauge, 0xFFFFu) == 0.0f);

    /* set_value() must not touch the geometry cache. */
    ws_snapshot = ws;
    lv_path_gauge_set_value(gauge, 25);
    lv_path_gauge_set_value(gauge, 100);
    lv_path_gauge_set_value(gauge, 0);
    TU_EXPECT(memcmp(&ws, &ws_snapshot, sizeof(ws)) == 0);

    /* Invalid paths are rejected and never leave stale geometry. */
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_multi_path, &ws) ==
              PG_ERR_INVALID_PATH);
    TU_EXPECT(lv_path_gauge_get_vertex_count(gauge) == 0u);
    TU_EXPECT(lv_path_gauge_get_total_distance(gauge) == 0.0f);
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_closed_path, &ws) ==
              PG_ERR_INVALID_PATH);
    TU_EXPECT(lv_path_gauge_get_vertex_count(gauge) == 0u);
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_bad_path, &ws) ==
              PG_ERR_INVALID_PATH);
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_degenerate_path, &ws) ==
              PG_ERR_DEGENERATE);
    TU_EXPECT(lv_path_gauge_get_vertex_count(gauge) == 0u);

    /* Workspace exhaustion is explicit; the gauge stays path-less. */
    lv_path_gauge_workspace_init(&ws_small, 0.01f);
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_wiggly_path, &ws_small) ==
              PG_ERR_WORKSPACE_TOO_SMALL);
    TU_EXPECT(lv_path_gauge_get_vertex_count(gauge) == 0u);
    TU_EXPECT(lv_path_gauge_set_path(gauge, NULL, &ws) == PG_OK);
    TU_EXPECT(lv_path_gauge_get_vertex_count(gauge) == 0u);

    /* Argument validation. */
    TU_EXPECT(lv_path_gauge_set_path(NULL, &g_s_path, &ws) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(lv_path_gauge_set_path(gauge, &g_s_path, NULL) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(lv_path_gauge_get_vertex_count(NULL) == 0u);
    TU_EXPECT(lv_path_gauge_get_total_distance(NULL) == 0.0f);
    TU_EXPECT(lv_path_gauge_get_value(NULL) == 0);
    TU_EXPECT(lv_path_gauge_get_min(NULL) == 0);
    TU_EXPECT(lv_path_gauge_get_max(NULL) == 0);
    TU_NEAR(lv_path_gauge_get_value_fraction(NULL), 0.0f, 1e-6f);

    /* A zeroed workspace (no init call) still works: tolerance falls back. */
    {
        static lv_path_gauge_workspace_t zero_ws;

        memset(&zero_ws, 0, sizeof(zero_ws));
        TU_EXPECT(lv_path_gauge_set_path(gauge, &g_s_path, &zero_ws) == PG_OK);
        TU_NEAR(zero_ws.tolerance, LV_PATH_GAUGE_DEFAULT_TOLERANCE, 1e-6f);
        TU_EXPECT(lv_path_gauge_get_vertex_count(gauge) > 2u);
    }

    free(td.frame);
    return TU_SUMMARY() ? 1 : 0;
}
