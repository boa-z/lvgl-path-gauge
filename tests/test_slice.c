/**
 * @file test_slice.c
 * @brief Arc-length slice extraction across commands, joints and subpaths.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "test_recorder.h"
#include "test_util.h"

#include "path2d/pg_bezier.h"
#include "path2d/pg_measure.h"
#include "path2d/pg_path.h"
#include "path2d/pg_writer.h"

#include <math.h>
#include <string.h>

#define SLICE_WS 1024u

static pg_measure_sample_t g_ws[SLICE_WS];
static tu_rec_t g_rec;

static float tu_dist(pg_point_t a, pg_point_t b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;

    return sqrtf(dx * dx + dy * dy);
}

/* Dense-sample distance from p to a cubic: high-precision geometry oracle. */
static float tu_dist_to_cubic(pg_cubic_t cubic, pg_point_t p)
{
    float best = 1e9f;
    int k;

    for (k = 0; k <= 4000; k++) {
        float t = (float)k / 4000.0f;
        pg_point_t q = pg_cubic_eval(cubic.p0, cubic.p1, cubic.p2, cubic.p3, t);
        float d = tu_dist(p, q);

        if (d < best) {
            best = d;
        }
    }
    return best;
}

/* Measures a recorded command list (must start with MOVE). */
static pg_result_t tu_measure_record(const tu_rec_t *rec, pg_measure_t *measure,
                                     pg_measure_sample_t *ws, uint16_t capacity,
                                     float tolerance)
{
    pg_path_t path;

    path.cmds = rec->cmds;
    path.cmd_count = rec->count;
    return pg_measure_init(measure, &path, ws, capacity, tolerance);
}

int main(void)
{
    static const pg_cmd_t line_path[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(100.0f, 0.0f),
    };
    static const pg_cmd_t quad_path[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_QUAD_TO(50.0f, 100.0f, 100.0f, 0.0f),
    };
    static const pg_cmd_t cubic_path[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_CUBIC_TO(0.0f, 100.0f, 100.0f, 100.0f, 100.0f, 0.0f),
    };
    static const pg_cmd_t mixed_path[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(50.0f, 0.0f),
        PG_QUAD_TO(75.0f, 50.0f, 100.0f, 0.0f),
        PG_CUBIC_TO(120.0f, -50.0f, 140.0f, 50.0f, 160.0f, 0.0f),
    };
    static const pg_cmd_t corner_path[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(100.0f, 0.0f),
        PG_LINE_TO(100.0f, 100.0f),
    };
    static const pg_cmd_t two_sub[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(10.0f, 0.0f),
        PG_MOVE_TO(100.0f, 0.0f),
        PG_LINE_TO(110.0f, 0.0f),
    };
    /* The break's coordinates coincide with the previous subpath end: the
     * contour boundary must still come from the MOVE command. */
    static const pg_cmd_t coincident_break[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(10.0f, 0.0f),
        PG_MOVE_TO(10.0f, 0.0f),
        PG_LINE_TO(20.0f, 0.0f),
    };
    pg_path_t path;
    pg_measure_t m;
    pg_path_writer_t writer;
    pg_measure_t sub_measure;
    float total;
    pg_point_t pos;
    pg_point_t tan;

    /* Full slice of a line: MOVE + LINE with identical points. */
    path.cmds = line_path;
    path.cmd_count = PG_ARRAY_SIZE(line_path);
    TU_EXPECT(pg_measure_init(&m, &path, g_ws, SLICE_WS, 0.5f) == PG_OK);
    tu_rec_reset(&g_rec);
    writer = tu_rec_writer(&g_rec);
    TU_EXPECT(pg_measure_slice(&m, 0.0f, pg_measure_get_length(&m), &writer) ==
              PG_OK);
    TU_EXPECT(g_rec.count == 2u);
    tu_expect_move(&g_rec, 0, 0.0f, 0.0f, 1e-5f);
    tu_expect_line(&g_rec, 1, 100.0f, 0.0f, 1e-5f);

    /* Full slice of quad/cubic keeps the original control points exactly. */
    path.cmds = quad_path;
    path.cmd_count = PG_ARRAY_SIZE(quad_path);
    TU_EXPECT(pg_measure_init(&m, &path, g_ws, SLICE_WS, 0.5f) == PG_OK);
    tu_rec_reset(&g_rec);
    TU_EXPECT(pg_measure_slice_normalized(&m, 0.0f, 1.0f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 2u);
    tu_expect_quad(&g_rec, 1, 50.0f, 100.0f, 100.0f, 0.0f, 1e-5f);

    path.cmds = cubic_path;
    path.cmd_count = PG_ARRAY_SIZE(cubic_path);
    TU_EXPECT(pg_measure_init(&m, &path, g_ws, SLICE_WS, 0.5f) == PG_OK);
    total = pg_measure_get_length(&m);
    tu_rec_reset(&g_rec);
    TU_EXPECT(pg_measure_slice_normalized(&m, 0.0f, 1.0f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 2u);
    tu_expect_cubic(&g_rec, 1, 0.0f, 100.0f, 100.0f, 100.0f, 100.0f, 0.0f,
                    1e-5f);

    /* Partial cubic slice 20%-80%: De Casteljau sub-curve stays a cubic, its
     * samples lie on the original curve and its length matches the range. */
    tu_rec_reset(&g_rec);
    TU_EXPECT(pg_measure_slice_normalized(&m, 0.2f, 0.8f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 2u);
    TU_EXPECT(g_rec.cmds[0].type == PG_CMD_MOVE);
    TU_EXPECT(g_rec.cmds[1].type == PG_CMD_CUBIC);
    TU_EXPECT(pg_measure_get_pos_tan_normalized(&m, 0.2f, &pos, &tan) == PG_OK);
    TU_POINT_NEAR(g_rec.cmds[0].p1, pos.x, pos.y, 1e-3f);
    TU_EXPECT(pg_measure_get_pos_tan_normalized(&m, 0.8f, &pos, NULL) == PG_OK);
    TU_POINT_NEAR(g_rec.cmds[1].p3, pos.x, pos.y, 1e-3f);
    {
        pg_cubic_t sub = { g_rec.cmds[0].p1, g_rec.cmds[1].p1, g_rec.cmds[1].p2,
                           g_rec.cmds[1].p3 };
        pg_cubic_t orig = { cubic_path[0].p1, cubic_path[1].p1,
                            cubic_path[1].p2, cubic_path[1].p3 };
        int k;

        for (k = 0; k <= 100; k++) {
            float u = (float)k / 100.0f;
            pg_point_t on_sub = pg_cubic_eval(sub.p0, sub.p1, sub.p2, sub.p3, u);

            TU_EXPECT(tu_dist_to_cubic(orig, on_sub) < 0.2f);
        }
    }
    {
        pg_measure_sample_t sub_ws[SLICE_WS];

        TU_EXPECT(tu_measure_record(&g_rec, &sub_measure, sub_ws, SLICE_WS,
                                    0.1f) == PG_OK);
        TU_NEAR(pg_measure_get_length(&sub_measure) / (0.6f * total), 1.0f,
                0.005f);
    }

    /* Cross-command slice (LINE -> QUAD -> partial CUBIC). */
    path.cmds = mixed_path;
    path.cmd_count = PG_ARRAY_SIZE(mixed_path);
    TU_EXPECT(pg_measure_init(&m, &path, g_ws, SLICE_WS, 0.1f) == PG_OK);
    total = pg_measure_get_length(&m);
    tu_rec_reset(&g_rec);
    TU_EXPECT(pg_measure_slice(&m, 25.0f, total - 10.0f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 4u);
    tu_expect_move(&g_rec, 0, 25.0f, 0.0f, 1e-3f);
    tu_expect_line(&g_rec, 1, 50.0f, 0.0f, 1e-3f);
    tu_expect_quad(&g_rec, 2, 75.0f, 50.0f, 100.0f, 0.0f, 1e-3f);
    TU_EXPECT(g_rec.cmds[3].type == PG_CMD_CUBIC);

    /* Command boundary: start exactly at the corner distance. */
    path.cmds = corner_path;
    path.cmd_count = PG_ARRAY_SIZE(corner_path);
    TU_EXPECT(pg_measure_init(&m, &path, g_ws, SLICE_WS, 0.5f) == PG_OK);
    tu_rec_reset(&g_rec);
    TU_EXPECT(pg_measure_slice(&m, 100.0f, 150.0f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 2u);
    tu_expect_move(&g_rec, 0, 100.0f, 0.0f, 1e-4f);
    tu_expect_line(&g_rec, 1, 100.0f, 50.0f, 1e-4f);

    /* Multi-subpath jump: the break becomes an extra MOVE, never a line. */
    path.cmds = two_sub;
    path.cmd_count = PG_ARRAY_SIZE(two_sub);
    TU_EXPECT(pg_measure_init(&m, &path, g_ws, SLICE_WS, 0.5f) == PG_OK);
    TU_NEAR(pg_measure_get_length(&m), 20.0f, 1e-4f);
    tu_rec_reset(&g_rec);
    TU_EXPECT(pg_measure_slice(&m, 5.0f, 15.0f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 4u);
    tu_expect_move(&g_rec, 0, 5.0f, 0.0f, 1e-4f);
    tu_expect_line(&g_rec, 1, 10.0f, 0.0f, 1e-4f);
    tu_expect_move(&g_rec, 2, 100.0f, 0.0f, 1e-4f);
    tu_expect_line(&g_rec, 3, 105.0f, 0.0f, 1e-4f);

    /* Slice starting exactly at the joint resolves to the later subpath. */
    tu_rec_reset(&g_rec);
    TU_EXPECT(pg_measure_slice(&m, 10.0f, 20.0f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 2u);
    tu_expect_move(&g_rec, 0, 100.0f, 0.0f, 1e-4f);
    tu_expect_line(&g_rec, 1, 110.0f, 0.0f, 1e-4f);

    /* Same-coordinate contour break: topology comes from the MOVE command,
     * not from comparing positions (which are identical here). */
    path.cmds = coincident_break;
    path.cmd_count = PG_ARRAY_SIZE(coincident_break);
    TU_EXPECT(pg_measure_init(&m, &path, g_ws, SLICE_WS, 0.5f) == PG_OK);
    TU_NEAR(pg_measure_get_length(&m), 20.0f, 1e-4f);
    tu_rec_reset(&g_rec);
    TU_EXPECT(pg_measure_slice_normalized(&m, 0.0f, 1.0f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 4u);
    tu_expect_move(&g_rec, 0, 0.0f, 0.0f, 1e-5f);
    tu_expect_line(&g_rec, 1, 10.0f, 0.0f, 1e-4f);
    tu_expect_move(&g_rec, 2, 10.0f, 0.0f, 1e-4f);
    tu_expect_line(&g_rec, 3, 20.0f, 0.0f, 1e-4f);
    /* A window that crosses the coincident break keeps both contours. */
    tu_rec_reset(&g_rec);
    TU_EXPECT(pg_measure_slice(&m, 5.0f, 15.0f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 4u);
    tu_expect_move(&g_rec, 0, 5.0f, 0.0f, 1e-4f);
    tu_expect_line(&g_rec, 1, 10.0f, 0.0f, 1e-4f);
    tu_expect_move(&g_rec, 2, 10.0f, 0.0f, 1e-4f);
    tu_expect_line(&g_rec, 3, 15.0f, 0.0f, 1e-4f);

    /* Zero-length slice: a single MOVE, at any position. */
    path.cmds = two_sub;
    path.cmd_count = PG_ARRAY_SIZE(two_sub);
    TU_EXPECT(pg_measure_init(&m, &path, g_ws, SLICE_WS, 0.5f) == PG_OK);
    tu_rec_reset(&g_rec);
    TU_EXPECT(pg_measure_slice(&m, 7.0f, 7.0f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 1u);
    tu_expect_move(&g_rec, 0, 7.0f, 0.0f, 1e-4f);
    tu_rec_reset(&g_rec);
    TU_EXPECT(pg_measure_slice(&m, 0.0f, 0.0f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 1u);
    tu_expect_move(&g_rec, 0, 0.0f, 0.0f, 1e-5f);
    tu_rec_reset(&g_rec);
    TU_EXPECT(pg_measure_slice_normalized(&m, 1.0f, 1.0f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 1u);
    tu_expect_move(&g_rec, 0, 110.0f, 0.0f, 1e-4f);

    /* Range clamping: a superset range equals the full slice. */
    tu_rec_reset(&g_rec);
    TU_EXPECT(pg_measure_slice(&m, -100.0f, 1e6f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 4u);

    /* Contract errors: NaN and start > end are rejected before clamping. */
    TU_EXPECT(pg_measure_slice(&m, tu_nan(), 5.0f, &writer) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_measure_slice(&m, 0.0f, tu_nan(), &writer) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_measure_slice(&m, 15.0f, 5.0f, &writer) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_measure_slice_normalized(&m, 0.8f, 0.2f, &writer) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_measure_slice_normalized(&m, tu_nan(), 0.2f, &writer) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_measure_slice_normalized(&m, 2.0f, 1.5f, &writer) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_measure_slice(NULL, 0.0f, 1.0f, &writer) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_measure_slice(&m, 0.0f, 1.0f, NULL) == PG_ERR_INVALID_ARG);

    /* Normalized out-of-range clamps to an empty (single MOVE) slice. */
    tu_rec_reset(&g_rec);
    TU_EXPECT(pg_measure_slice_normalized(&m, -5.0f, -3.0f, &writer) == PG_OK);
    TU_EXPECT(g_rec.count == 1u);
    tu_expect_move(&g_rec, 0, 0.0f, 0.0f, 1e-5f);

    /* Normalized and absolute slices agree command by command. */
    {
        static tu_rec_t rec_abs;
        static tu_rec_t rec_norm;
        pg_path_writer_t w_abs;
        pg_path_writer_t w_norm;
        unsigned i;

        tu_rec_reset(&rec_abs);
        tu_rec_reset(&rec_norm);
        w_abs = tu_rec_writer(&rec_abs);
        w_norm = tu_rec_writer(&rec_norm);
        TU_EXPECT(pg_measure_slice(&m, 0.2f * 20.0f, 0.8f * 20.0f, &w_abs) ==
                  PG_OK);
        TU_EXPECT(pg_measure_slice_normalized(&m, 0.2f, 0.8f, &w_norm) ==
                  PG_OK);
        TU_EXPECT(rec_abs.count == rec_norm.count);
        TU_EXPECT(rec_abs.count == 4u); /* MOVE, LINE, MOVE, LINE */
        for (i = 0; i < rec_abs.count && i < rec_norm.count; i++) {
            TU_EXPECT(rec_abs.cmds[i].type == rec_norm.cmds[i].type);
            TU_POINT_NEAR(rec_abs.cmds[i].p1, rec_norm.cmds[i].p1.x,
                          rec_norm.cmds[i].p1.y, 1e-5f);
            TU_POINT_NEAR(rec_abs.cmds[i].p3, rec_norm.cmds[i].p3.x,
                          rec_norm.cmds[i].p3.y, 1e-5f);
        }
    }

    /* Writer failure aborts the slice and is propagated. */
    {
        static tu_rec_t failing;

        tu_rec_reset(&failing);
        failing.fail_with = PG_ERR_WORKSPACE_TOO_SMALL;
        writer = tu_rec_writer(&failing);
        TU_EXPECT(pg_measure_slice(&m, 0.0f, 5.0f, &writer) ==
                  PG_ERR_WORKSPACE_TOO_SMALL);
        TU_EXPECT(failing.count == 0u && failing.calls == 1u);
    }

    /* Incomplete writer (missing callback) is rejected up front. */
    {
        pg_path_writer_t incomplete = tu_rec_writer(&g_rec);

        incomplete.quad_to = NULL;
        TU_EXPECT(pg_measure_slice(&m, 0.0f, 5.0f, &incomplete) ==
                  PG_ERR_INVALID_ARG);
    }

    /* Uninitialized measure is rejected. */
    {
        pg_measure_t zeroed;

        memset(&zeroed, 0, sizeof(zeroed));
        writer = tu_rec_writer(&g_rec);
        TU_EXPECT(pg_measure_slice(&zeroed, 0.0f, 1.0f, &writer) ==
                  PG_ERR_INVALID_ARG);
    }

    /* Workspace exhaustion through the buffer writer: explicit error, the
     * buffer is poisoned and can never be exported. */
    {
        pg_cmd_t cmds[1];
        pg_path_buffer_t buffer;
        pg_path_writer_t buffer_writer;
        pg_path_t out;

        TU_EXPECT(pg_path_buffer_init(&buffer, cmds, 1u) == PG_OK);
        buffer_writer = pg_path_buffer_writer(&buffer);
        TU_EXPECT(pg_measure_slice(&m, 5.0f, 15.0f, &buffer_writer) ==
                  PG_ERR_WORKSPACE_TOO_SMALL);
        TU_EXPECT(buffer.poisoned && buffer.count == 1u &&
                  buffer.failure == PG_ERR_WORKSPACE_TOO_SMALL);
        out.cmds = cmds;
        out.cmd_count = 1u;
        TU_EXPECT(pg_path_buffer_to_path(&buffer, &out) ==
                  PG_ERR_WORKSPACE_TOO_SMALL);
        TU_EXPECT(out.cmds == NULL && out.cmd_count == 0u);
    }

    /* An empty writer (NULL buffer) is rejected without crashing. */
    {
        pg_path_writer_t empty_writer = pg_path_buffer_writer(NULL);

        TU_EXPECT(pg_measure_slice(&m, 0.0f, 5.0f, &empty_writer) ==
                  PG_ERR_INVALID_ARG);
    }

    /* Degenerate source measure (zero total length) is rejected. */
    {
        pg_measure_t degenerate = m;

        degenerate.total_length = 0.0f;
        writer = tu_rec_writer(&g_rec);
        TU_EXPECT(pg_measure_slice(&degenerate, 0.0f, 1.0f, &writer) ==
                  PG_ERR_DEGENERATE);
        TU_EXPECT(pg_measure_slice_normalized(&degenerate, 0.0f, 1.0f,
                                              &writer) ==
                  PG_ERR_DEGENERATE);
    }

    return TU_SUMMARY() ? 1 : 0;
}
