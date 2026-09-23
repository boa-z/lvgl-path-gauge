/**
 * @file test_flatten.c
 * @brief Writer-based flatten: contour output, tolerance, degenerate cases.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "test_recorder.h"
#include "test_util.h"

#include "path2d/pg_bezier.h"
#include "path2d/pg_flatten.h"
#include "path2d/pg_path.h"

#include <math.h>

static tu_rec_t g_rec;

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
    /* Two subpaths must stay distinguishable in the writer stream. */
    static const pg_cmd_t two_sub[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(10.0f, 0.0f),
        PG_MOVE_TO(100.0f, 0.0f),
        PG_LINE_TO(110.0f, 0.0f),
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
    pg_path_t two = { two_sub, PG_ARRAY_SIZE(two_sub) };
    pg_path_t empty = { empty_cmds, 0 };
    pg_path_writer_t writer;

    /* A line flattens to MOVE + LINE. */
    tu_rec_reset(&g_rec);
    writer = tu_rec_writer(&g_rec);
    TU_EXPECT(pg_path_flatten(&line, 0.5f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 2u);
    TU_EXPECT(tu_rec_kind_count(&g_rec, PG_CMD_MOVE) == 1u);
    TU_EXPECT(tu_rec_kind_count(&g_rec, PG_CMD_LINE) == 1u);
    tu_expect_move(&g_rec, 0, 0.0f, 0.0f, 1e-6f);
    tu_expect_line(&g_rec, 1, 100.0f, 0.0f, 1e-6f);
    TU_NEAR(tu_rec_length(&g_rec), 100.0f, 1e-4f);

    /* MOVE-only emits exactly one move_to. */
    tu_rec_reset(&g_rec);
    writer = tu_rec_writer(&g_rec);
    TU_EXPECT(pg_path_flatten(&moves, 0.5f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 1u);
    tu_expect_move(&g_rec, 0, 1.0f, 2.0f, 1e-6f);

    /* CLOSE comes back as a line_to to the subpath start. */
    tu_rec_reset(&g_rec);
    writer = tu_rec_writer(&g_rec);
    TU_EXPECT(pg_path_flatten(&sq, 0.5f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 5u);
    TU_EXPECT(tu_rec_kind_count(&g_rec, PG_CMD_MOVE) == 1u);
    TU_EXPECT(tu_rec_kind_count(&g_rec, PG_CMD_LINE) == 4u);
    tu_expect_move(&g_rec, 0, 0.0f, 0.0f, 1e-6f);
    tu_expect_line(&g_rec, 4, 0.0f, 0.0f, 1e-6f);
    TU_NEAR(tu_rec_length(&g_rec), 40.0f, 1e-4f);

    /* Subpath boundaries survive as separate move_to calls. */
    tu_rec_reset(&g_rec);
    writer = tu_rec_writer(&g_rec);
    TU_EXPECT(pg_path_flatten(&two, 0.5f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 4u);
    TU_EXPECT(tu_rec_kind_count(&g_rec, PG_CMD_MOVE) == 2u);
    tu_expect_move(&g_rec, 0, 0.0f, 0.0f, 1e-6f);
    tu_expect_line(&g_rec, 1, 10.0f, 0.0f, 1e-6f);
    tu_expect_move(&g_rec, 2, 100.0f, 0.0f, 1e-6f);
    tu_expect_line(&g_rec, 3, 110.0f, 0.0f, 1e-6f);
    /* Length stops at contour breaks: 10 + 10, never 10 + 90 + 10. */
    TU_NEAR(tu_rec_length(&g_rec), 20.0f, 1e-4f);

    /* Smooth curve: bounded vertex count, correct endpoints, no NaN. */
    tu_rec_reset(&g_rec);
    writer = tu_rec_writer(&g_rec);
    TU_EXPECT(pg_path_flatten(&cv, 0.5f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count > 5u && g_rec.count < 800u);
    tu_expect_move(&g_rec, 0, 0.0f, 0.0f, 1e-6f);
    TU_POINT_NEAR(tu_rec_end(&g_rec, g_rec.count - 1u), 100.0f, 0.0f, 1e-3f);
    {
        unsigned i;

        for (i = 0; i < g_rec.count; i++) {
            pg_point_t p = tu_rec_end(&g_rec, i);

            TU_EXPECT(p.x == p.x && p.y == p.y);
        }
    }

    /* Monotone collinear spans flatten in one step (no over-subdivision). */
    tu_rec_reset(&g_rec);
    writer = tu_rec_writer(&g_rec);
    TU_EXPECT(pg_path_flatten(&mq, 0.5f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 2u);
    tu_rec_reset(&g_rec);
    writer = tu_rec_writer(&g_rec);
    TU_EXPECT(pg_path_flatten(&mc, 0.5f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 2u);

    /* Collinear backtracking quad (the historical flatness bug): the chord is
     * (0,0)->(10,0) but the curve overshoots to x ~ 52.63 before returning;
     * the emitted polyline must follow the overshoot and its length must match
     * the dense oracle (arc length ~ 95.2632, not the chord 10). */
    tu_rec_reset(&g_rec);
    writer = tu_rec_writer(&g_rec);
    TU_EXPECT(pg_path_flatten(&bq, 0.25f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count > 4u);
    /* True turning point 52.6316; vertices straddle it, so allow 0.5. */
    TU_EXPECT(tu_rec_max_x(&g_rec) > 52.1f && tu_rec_max_x(&g_rec) < 52.7f);
    TU_NEAR(tu_rec_length(&g_rec), 95.2632f, 0.19f); /* 0.2% of the oracle */

    /* Collinear backtracking cubic: controls sit on the chord line, so only
     * the polygon-excess term can trigger subdivision. */
    tu_rec_reset(&g_rec);
    writer = tu_rec_writer(&g_rec);
    TU_EXPECT(pg_path_flatten(&bc, 0.25f, &writer) == PG_OK);
    {
        float oracle = tu_cubic_oracle((pg_point_t){ 0.0f, 0.0f },
                                      (pg_point_t){ 100.0f, 0.0f },
                                      (pg_point_t){ -50.0f, 0.0f },
                                      (pg_point_t){ 50.0f, 0.0f });

        TU_NEAR(tu_rec_length(&g_rec) / oracle, 1.0f, 0.002f);
    }

    /* Absurd tolerance is clamped: terminates with bounded output. */
    tu_rec_reset(&g_rec);
    writer = tu_rec_writer(&g_rec);
    TU_EXPECT(pg_path_flatten(&cv, 1e-9f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count < 5000u);

    /* Sink errors abort immediately and are propagated unchanged. */
    tu_rec_reset(&g_rec);
    g_rec.fail_with = PG_ERR_WORKSPACE_TOO_SMALL;
    writer = tu_rec_writer(&g_rec);
    TU_EXPECT(pg_path_flatten(&cv, 0.25f, &writer) ==
              PG_ERR_WORKSPACE_TOO_SMALL);
    TU_EXPECT(g_rec.count == 0u && g_rec.calls == 1u);

    /* Argument and callback validation. */
    tu_rec_reset(&g_rec);
    writer = tu_rec_writer(&g_rec);
    TU_EXPECT(pg_path_flatten(NULL, 0.5f, &writer) == PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_path_flatten(&line, 0.5f, NULL) == PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_path_flatten(&line, 0.0f, &writer) == PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_path_flatten(&line, -1.0f, &writer) == PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_path_flatten(&empty, 0.5f, &writer) == PG_ERR_INVALID_PATH);
    {
        pg_path_writer_t incomplete = tu_rec_writer(&g_rec);

        incomplete.line_to = NULL;
        TU_EXPECT(pg_path_flatten(&line, 0.5f, &incomplete) ==
                  PG_ERR_INVALID_ARG);
        incomplete = tu_rec_writer(&g_rec);
        incomplete.move_to = NULL;
        TU_EXPECT(pg_path_flatten(&line, 0.5f, &incomplete) ==
                  PG_ERR_INVALID_ARG);
    }

    return TU_SUMMARY() ? 1 : 0;
}
