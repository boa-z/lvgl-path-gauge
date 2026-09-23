/* SPDX-License-Identifier: MIT */
/* Adaptive flatten: vertex emission, tolerance, degenerate handling. */
#include "test_util.h"
#include "path2d/pg_flatten.h"
#include "path2d/pg_path.h"

#define CAP 4096u

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
    static const pg_cmd_t empty_cmds[] = {
        PG_MOVE_TO(0.0f, 0.0f),
    };
    pg_path_t line = { line_cmds, PG_ARRAY_SIZE(line_cmds) };
    pg_path_t moves = { move_only, PG_ARRAY_SIZE(move_only) };
    pg_path_t sq = { square, PG_ARRAY_SIZE(square) };
    pg_path_t cv = { curve, PG_ARRAY_SIZE(curve) };
    pg_path_t empty = { empty_cmds, 0 };
    unsigned i;

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
    TU_EXPECT(g_n > 4u && g_n < 500u);
    TU_POINT_NEAR(g_pts[0], 0.0f, 0.0f, 1e-6f);
    TU_POINT_NEAR(g_pts[g_n - 1u], 100.0f, 0.0f, 1e-3f);
    for (i = 0; i < g_n; i++) {
        TU_EXPECT(g_pts[i].x == g_pts[i].x); /* no NaN */
        TU_EXPECT(g_pts[i].y == g_pts[i].y);
    }

    /* Absurd tolerance is clamped: terminates with bounded output. */
    tu_reset();
    TU_EXPECT(pg_path_flatten(&cv, 1e-9f, tu_collect, NULL) == PG_OK);
    TU_EXPECT(g_n < 2000u);

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
