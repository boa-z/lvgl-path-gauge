/**
 * @file test_flatten.c
 * @brief Adaptive flatten: vertices, tolerance, degenerate and collinear cases.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "test_util.h"

#include "path2d/pg_bezier.h"
#include "path2d/pg_flatten.h"
#include "path2d/pg_path.h"

#include <math.h>

#define CAP 8192u

static pg_point_t g_pts[CAP];
static unsigned g_n;

static void tu_collect(void *ctx, pg_point_t p)
{
    (void)ctx;
    if (g_n < CAP) {
        g_pts[g_n++] = p;
    }
}

static void tu_reset(void)
{
    g_n = 0;
}

static float tu_polyline_length(void)
{
    float total = 0.0f;
    unsigned i;

    for (i = 1; i < g_n; i++) {
        float dx = g_pts[i].x - g_pts[i - 1].x;
        float dy = g_pts[i].y - g_pts[i - 1].y;

        total += sqrtf(dx * dx + dy * dy);
    }
    return total;
}

/* Dense uniform-t sampling oracle for a single cubic command. */
static float tu_cubic_oracle(pg_point_t p0, pg_point_t p1, pg_point_t p2,
                             pg_point_t p3)
{
    pg_point_t prev = p0;
    float total = 0.0f;
    int k;

    for (k = 1; k <= 20000; k++) {
        float t = (float)k / 20000.0f;
        pg_point_t p = pg_cubic_eval(p0, p1, p2, p3, t);
        float dx = p.x - prev.x;
        float dy = p.y - prev.y;

        total += sqrtf(dx * dx + dy * dy);
        prev = p;
    }
    return total;
}

int main(void)
{
    static const pg_cmd_t line_cmds[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(100.0f, 0.0f),
    };
    static const pg_cmd_t move_only[] = {
        PG_MOVE_TO(1.0f, 2.0f),
    };
    static const pg_cmd_t square[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(10.0f, 0.0f),
        PG_LINE_TO(10.0f, 10.0f),
        PG_LINE_TO(0.0f, 10.0f),
        PG_CLOSE(),
    };
    static const pg_cmd_t curve[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_CUBIC_TO(0.0f, 100.0f, 100.0f, 100.0f, 100.0f, 0.0f),
    };
    static const pg_cmd_t monotone_quad[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_QUAD_TO(50.0f, 0.0f, 100.0f, 0.0f),
    };
    static const pg_cmd_t monotone_cubic[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_CUBIC_TO(33.0f, 0.0f, 66.0f, 0.0f, 100.0f, 0.0f),
    };
    static const pg_cmd_t backtrack_quad[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_QUAD_TO(100.0f, 0.0f, 10.0f, 0.0f),
    };
    static const pg_cmd_t backtrack_cubic[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_CUBIC_TO(100.0f, 0.0f, -50.0f, 0.0f, 50.0f, 0.0f),
    };
    static const pg_cmd_t empty_cmds[] = {
        PG_MOVE_TO(0.0f, 0.0f),
    };
    pg_path_t line = { line_cmds, PG_ARRAY_SIZE(line_cmds) };
    pg_path_t moves = { move_only, PG_ARRAY_SIZE(move_only) };
    pg_path_t sq = { square, PG_ARRAY_SIZE(square) };
    pg_path_t cv = { curve, PG_ARRAY_SIZE(curve) };
    pg_path_t mq = { monotone_quad, PG_ARRAY_SIZE(monotone_quad) };
    pg_path_t mc = { monotone_cubic, PG_ARRAY_SIZE(monotone_cubic) };
    pg_path_t bq = { backtrack_quad, PG_ARRAY_SIZE(backtrack_quad) };
    pg_path_t bc = { backtrack_cubic, PG_ARRAY_SIZE(backtrack_cubic) };
    pg_path_t empty = { empty_cmds, 0 };
    unsigned i;
    float max_x;

    tu_reset();
    TU_EXPECT(pg_path_flatten(&line, 0.5f, tu_collect, NULL) == PG_OK);
    TU_EXPECT(g_n == 2u);
    TU_POINT_NEAR(g_pts[0], 0.0f, 0.0f, 1e-6f);
    TU_POINT_NEAR(g_pts[1], 100.0f, 0.0f, 1e-6f);

    tu_reset();
    TU_EXPECT(pg_path_flatten(&moves, 0.5f, tu_collect, NULL) == PG_OK);
    TU_EXPECT(g_n == 1u);
    TU_POINT_NEAR(g_pts[0], 1.0f, 2.0f, 1e-6f);

    tu_reset();
    TU_EXPECT(pg_path_flatten(&sq, 0.5f, tu_collect, NULL) == PG_OK);
    TU_EXPECT(g_n == 5u);
    TU_POINT_NEAR(g_pts[0], 0.0f, 0.0f, 1e-6f);
    TU_POINT_NEAR(g_pts[4], 0.0f, 0.0f, 1e-6f);

    tu_reset();
    TU_EXPECT(pg_path_flatten(&cv, 0.5f, tu_collect, NULL) == PG_OK);
    TU_EXPECT(g_n > 4u && g_n < 800u);
    TU_POINT_NEAR(g_pts[0], 0.0f, 0.0f, 1e-6f);
    TU_POINT_NEAR(g_pts[g_n - 1u], 100.0f, 0.0f, 1e-3f);
    for (i = 0; i < g_n; i++) {
        TU_EXPECT(g_pts[i].x == g_pts[i].x); /* no NaN */
        TU_EXPECT(g_pts[i].y == g_pts[i].y);
    }

    /* Monotone collinear spans must flatten in one step: perpendicular
     * deviation and polygon excess are both zero, so no over-subdivision. */
    tu_reset();
    TU_EXPECT(pg_path_flatten(&mq, 0.5f, tu_collect, NULL) == PG_OK);
    TU_EXPECT(g_n == 2u);
    TU_POINT_NEAR(g_pts[1], 100.0f, 0.0f, 1e-4f);
    tu_reset();
    TU_EXPECT(pg_path_flatten(&mc, 0.5f, tu_collect, NULL) == PG_OK);
    TU_EXPECT(g_n == 2u);

    /* Collinear backtracking quad (the historical flatness bug): the chord is
     * (0,0)->(10,0) but the curve overshoots to x ~ 52.63 before returning;
     * the emitted polyline must follow the overshoot and its length must match
     * the dense oracle (arc length ~ 95.2632, not the chord 10). */
    tu_reset();
    TU_EXPECT(pg_path_flatten(&bq, 0.25f, tu_collect, NULL) == PG_OK);
    TU_EXPECT(g_n > 4u);
    max_x = g_pts[0].x;
    for (i = 1; i < g_n; i++) {
        if (g_pts[i].x > max_x) {
            max_x = g_pts[i].x;
        }
    }
    /* True turning point 52.6316; vertices straddle it, so allow 0.5. */
    TU_EXPECT(max_x > 52.1f && max_x < 52.7f);
    TU_NEAR(tu_polyline_length(), 95.2632f, 0.19f); /* 0.2% of the oracle */

    /* Collinear backtracking cubic: controls sit on the chord line, so only
     * the polygon-excess term can trigger subdivision. */
    tu_reset();
    TU_EXPECT(pg_path_flatten(&bc, 0.25f, tu_collect, NULL) == PG_OK);
    {
        float oracle = tu_cubic_oracle((pg_point_t){ 0.0f, 0.0f },
                                      (pg_point_t){ 100.0f, 0.0f },
                                      (pg_point_t){ -50.0f, 0.0f },
                                      (pg_point_t){ 50.0f, 0.0f });

        TU_NEAR(tu_polyline_length() / oracle, 1.0f, 0.002f);
    }

    /* Absurd tolerance is clamped: terminates with bounded output. */
    tu_reset();
    TU_EXPECT(pg_path_flatten(&cv, 1e-9f, tu_collect, NULL) == PG_OK);
    TU_EXPECT(g_n < 5000u);

    TU_EXPECT(pg_path_flatten(NULL, 0.5f, tu_collect, NULL) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_path_flatten(&line, 0.5f, NULL, NULL) == PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_path_flatten(&line, 0.0f, tu_collect, NULL) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_path_flatten(&line, -1.0f, tu_collect, NULL) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_path_flatten(&empty, 0.5f, tu_collect, NULL) ==
              PG_ERR_INVALID_PATH);

    return TU_SUMMARY() ? 1 : 0;
}
