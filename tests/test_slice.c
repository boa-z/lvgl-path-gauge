/**
 * @file test_slice.c
 * @brief Arc-length slice extraction across commands, joints and subpaths.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "test_util.h"

#include "path2d/pg_bezier.h"
#include "path2d/pg_measure.h"
#include "path2d/pg_path.h"
#include "path2d/pg_writer.h"

#include <math.h>
#include <string.h>

#define REC_MAX 64u
#define SLICE_WS 1024u

typedef struct {
    pg_cmd_t cmds[REC_MAX]; /**< Recorded commands. */
    uint16_t count;         /**< Commands recorded. */
    bool fail_fast;         /**< When set every callback fails. */
} rec_t;

static pg_measure_sample_t g_ws[SLICE_WS];

static float tu_dist(pg_point_t a, pg_point_t b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;

    return sqrtf(dx * dx + dy * dy);
}

static pg_result_t rec_append(rec_t *rec, const pg_cmd_t *cmd)
{
    if (rec->fail_fast) {
        return PG_ERR_WORKSPACE_TOO_SMALL;
    }
    if (cmd->type != PG_CMD_MOVE && rec->count == 0u) {
        return PG_ERR_INVALID_ARG;
    }
    if (rec->count >= REC_MAX) {
        return PG_ERR_WORKSPACE_TOO_SMALL;
    }
    rec->cmds[rec->count] = *cmd;
    rec->count++;
    return PG_OK;
}

static pg_result_t rec_move(void *ctx, pg_point_t to)
{
    pg_cmd_t cmd = { PG_CMD_MOVE, to, { 0.0f, 0.0f }, { 0.0f, 0.0f } };

    return rec_append(ctx, &cmd);
}

static pg_result_t rec_line(void *ctx, pg_point_t to)
{
    pg_cmd_t cmd = { PG_CMD_LINE, to, { 0.0f, 0.0f }, { 0.0f, 0.0f } };

    return rec_append(ctx, &cmd);
}

static pg_result_t rec_quad(void *ctx, pg_point_t control, pg_point_t to)
{
    pg_cmd_t cmd = { PG_CMD_QUAD, control, to, { 0.0f, 0.0f } };

    return rec_append(ctx, &cmd);
}

static pg_result_t rec_cubic(void *ctx, pg_point_t c1, pg_point_t c2,
                             pg_point_t to)
{
    pg_cmd_t cmd = { PG_CMD_CUBIC, c1, c2, to };

    return rec_append(ctx, &cmd);
}

static pg_path_writer_t rec_writer(rec_t *rec)
{
    pg_path_writer_t writer = { rec_move, rec_line, rec_quad, rec_cubic, rec };

    return writer;
}

static void tu_expect_move(rec_t *rec, unsigned index, float x, float y,
                           float eps)
{
    TU_EXPECT(index < rec->count);
    if (index >= rec->count) {
        return;
    }
    TU_EXPECT(rec->cmds[index].type == PG_CMD_MOVE);
    TU_POINT_NEAR(rec->cmds[index].p1, x, y, eps);
}

static void tu_expect_line(rec_t *rec, unsigned index, float x, float y,
                           float eps)
{
    TU_EXPECT(index < rec->count);
    if (index >= rec->count) {
        return;
    }
    TU_EXPECT(rec->cmds[index].type == PG_CMD_LINE);
    TU_POINT_NEAR(rec->cmds[index].p1, x, y, eps);
}

static void tu_expect_quad(rec_t *rec, unsigned index, float cx, float cy,
                           float x, float y, float eps)
{
    TU_EXPECT(index < rec->count);
    if (index >= rec->count) {
        return;
    }
    TU_EXPECT(rec->cmds[index].type == PG_CMD_QUAD);
    TU_POINT_NEAR(rec->cmds[index].p1, cx, cy, eps);
    TU_POINT_NEAR(rec->cmds[index].p2, x, y, eps);
}

static void tu_expect_cubic(rec_t *rec, unsigned index, float c1x, float c1y,
                            float c2x, float c2y, float x, float y, float eps)
{
    TU_EXPECT(index < rec->count);
    if (index >= rec->count) {
        return;
    }
    TU_EXPECT(rec->cmds[index].type == PG_CMD_CUBIC);
    TU_POINT_NEAR(rec->cmds[index].p1, c1x, c1y, eps);
    TU_POINT_NEAR(rec->cmds[index].p2, c2x, c2y, eps);
    TU_POINT_NEAR(rec->cmds[index].p3, x, y, eps);
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
static pg_result_t tu_measure_path(const rec_t *rec, pg_measure_t *measure,
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
    pg_path_t path;
    pg_measure_t m;
    rec_t rec;
    pg_path_writer_t writer;
    pg_measure_t sub_measure;
    float total;
    pg_point_t pos;
    pg_point_t tan;

    /* Full slice of a line: MOVE + LINE with identical points. */
    path.cmds = line_path;
    path.cmd_count = PG_ARRAY_SIZE(line_path);
    TU_EXPECT(pg_measure_init(&m, &path, g_ws, SLICE_WS, 0.5f) == PG_OK);
    memset(&rec, 0, sizeof(rec));
    writer = rec_writer(&rec);
    TU_EXPECT(pg_measure_slice(&m, 0.0f, pg_measure_get_length(&m), &writer) ==
              PG_OK);
    TU_EXPECT(rec.count == 2u);
    tu_expect_move(&rec, 0, 0.0f, 0.0f, 1e-5f);
    tu_expect_line(&rec, 1, 100.0f, 0.0f, 1e-5f);

    /* Full slice of quad/cubic keeps the original control points exactly. */
    path.cmds = quad_path;
    path.cmd_count = PG_ARRAY_SIZE(quad_path);
    TU_EXPECT(pg_measure_init(&m, &path, g_ws, SLICE_WS, 0.5f) == PG_OK);
    memset(&rec, 0, sizeof(rec));
    TU_EXPECT(pg_measure_slice_normalized(&m, 0.0f, 1.0f, &writer) == PG_OK);
    TU_EXPECT(rec.count == 2u);
    tu_expect_quad(&rec, 1, 50.0f, 100.0f, 100.0f, 0.0f, 1e-5f);

    path.cmds = cubic_path;
    path.cmd_count = PG_ARRAY_SIZE(cubic_path);
    TU_EXPECT(pg_measure_init(&m, &path, g_ws, SLICE_WS, 0.5f) == PG_OK);
    total = pg_measure_get_length(&m);
    memset(&rec, 0, sizeof(rec));
    TU_EXPECT(pg_measure_slice_normalized(&m, 0.0f, 1.0f, &writer) == PG_OK);
    TU_EXPECT(rec.count == 2u);
    tu_expect_cubic(&rec, 1, 0.0f, 100.0f, 100.0f, 100.0f, 100.0f, 0.0f,
                    1e-5f);

    /* Partial cubic slice 20%-80%: De Casteljau sub-curve stays a cubic, its
     * samples lie on the original curve and its length matches the range. */
    memset(&rec, 0, sizeof(rec));
    TU_EXPECT(pg_measure_slice_normalized(&m, 0.2f, 0.8f, &writer) == PG_OK);
    TU_EXPECT(rec.count == 2u);
    TU_EXPECT(rec.cmds[0].type == PG_CMD_MOVE);
    TU_EXPECT(rec.cmds[1].type == PG_CMD_CUBIC);
    TU_EXPECT(pg_measure_get_pos_tan_normalized(&m, 0.2f, &pos, &tan) == PG_OK);
    TU_POINT_NEAR(rec.cmds[0].p1, pos.x, pos.y, 1e-3f);
    TU_EXPECT(pg_measure_get_pos_tan_normalized(&m, 0.8f, &pos, NULL) == PG_OK);
    TU_POINT_NEAR(rec.cmds[1].p3, pos.x, pos.y, 1e-3f);
    {
        pg_cubic_t sub = { rec.cmds[0].p1, rec.cmds[1].p1, rec.cmds[1].p2,
                           rec.cmds[1].p3 };
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

        TU_EXPECT(tu_measure_path(&rec, &sub_measure, sub_ws, SLICE_WS, 0.1f) ==
                  PG_OK);
        TU_NEAR(pg_measure_get_length(&sub_measure) / (0.6f * total), 1.0f,
                0.005f);
    }

    /* Cross-command slice (LINE -> QUAD -> partial CUBIC). */
    path.cmds = mixed_path;
    path.cmd_count = PG_ARRAY_SIZE(mixed_path);
    TU_EXPECT(pg_measure_init(&m, &path, g_ws, SLICE_WS, 0.1f) == PG_OK);
    total = pg_measure_get_length(&m);
    memset(&rec, 0, sizeof(rec));
    TU_EXPECT(pg_measure_slice(&m, 25.0f, total - 10.0f, &writer) == PG_OK);
    TU_EXPECT(rec.count == 4u);
    tu_expect_move(&rec, 0, 25.0f, 0.0f, 1e-3f);
    tu_expect_line(&rec, 1, 50.0f, 0.0f, 1e-3f);
    tu_expect_quad(&rec, 2, 75.0f, 50.0f, 100.0f, 0.0f, 1e-3f);
    TU_EXPECT(rec.cmds[3].type == PG_CMD_CUBIC);
    TU_EXPECT(rec.count == 4u);

    /* Command boundary: start exactly at the corner distance. */
    path.cmds = corner_path;
    path.cmd_count = PG_ARRAY_SIZE(corner_path);
    TU_EXPECT(pg_measure_init(&m, &path, g_ws, SLICE_WS, 0.5f) == PG_OK);
    memset(&rec, 0, sizeof(rec));
    TU_EXPECT(pg_measure_slice(&m, 100.0f, 150.0f, &writer) == PG_OK);
    TU_EXPECT(rec.count == 2u);
    tu_expect_move(&rec, 0, 100.0f, 0.0f, 1e-4f);
    tu_expect_line(&rec, 1, 100.0f, 50.0f, 1e-4f);

    /* Multi-subpath: the jump becomes an extra MOVE, never a drawn line. */
    path.cmds = two_sub;
    path.cmd_count = PG_ARRAY_SIZE(two_sub);
    TU_EXPECT(pg_measure_init(&m, &path, g_ws, SLICE_WS, 0.5f) == PG_OK);
    TU_NEAR(pg_measure_get_length(&m), 20.0f, 1e-4f);
    memset(&rec, 0, sizeof(rec));
    TU_EXPECT(pg_measure_slice(&m, 5.0f, 15.0f, &writer) == PG_OK);
    TU_EXPECT(rec.count == 4u);
    tu_expect_move(&rec, 0, 5.0f, 0.0f, 1e-4f);
    tu_expect_line(&rec, 1, 10.0f, 0.0f, 1e-4f);
    tu_expect_move(&rec, 2, 100.0f, 0.0f, 1e-4f);
    tu_expect_line(&rec, 3, 105.0f, 0.0f, 1e-4f);

    /* Slice starting exactly at the joint resolves to the later subpath. */
    memset(&rec, 0, sizeof(rec));
    TU_EXPECT(pg_measure_slice(&m, 10.0f, 20.0f, &writer) == PG_OK);
    TU_EXPECT(rec.count == 2u);
    tu_expect_move(&rec, 0, 100.0f, 0.0f, 1e-4f);
    tu_expect_line(&rec, 1, 110.0f, 0.0f, 1e-4f);

    /* Zero-length slice: a single MOVE, at any position. */
    memset(&rec, 0, sizeof(rec));
    TU_EXPECT(pg_measure_slice(&m, 7.0f, 7.0f, &writer) == PG_OK);
    TU_EXPECT(rec.count == 1u);
    tu_expect_move(&rec, 0, 7.0f, 0.0f, 1e-4f);
    memset(&rec, 0, sizeof(rec));
    TU_EXPECT(pg_measure_slice(&m, 0.0f, 0.0f, &writer) == PG_OK);
    TU_EXPECT(rec.count == 1u);
    tu_expect_move(&rec, 0, 0.0f, 0.0f, 1e-5f);
    memset(&rec, 0, sizeof(rec));
    TU_EXPECT(pg_measure_slice_normalized(&m, 1.0f, 1.0f, &writer) == PG_OK);
    TU_EXPECT(rec.count == 1u);
    tu_expect_move(&rec, 0, 110.0f, 0.0f, 1e-4f);

    /* Range clamping: a superset range equals the full slice. */
    memset(&rec, 0, sizeof(rec));
    TU_EXPECT(pg_measure_slice(&m, -100.0f, 1e6f, &writer) == PG_OK);
    TU_EXPECT(rec.count == 4u);

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
    memset(&rec, 0, sizeof(rec));
    TU_EXPECT(pg_measure_slice_normalized(&m, -5.0f, -3.0f, &writer) == PG_OK);
    TU_EXPECT(rec.count == 1u);
    tu_expect_move(&rec, 0, 0.0f, 0.0f, 1e-5f);

    /* Normalized and absolute slices agree command by command. */
    {
        rec_t rec_abs;
        rec_t rec_norm;
        pg_path_writer_t w_abs;
        pg_path_writer_t w_norm;
        unsigned i;

        memset(&rec_abs, 0, sizeof(rec_abs));
        memset(&rec_norm, 0, sizeof(rec_norm));
        w_abs = rec_writer(&rec_abs);
        w_norm = rec_writer(&rec_norm);
        TU_EXPECT(pg_measure_slice(&m, 0.2f * 20.0f, 0.8f * 20.0f, &w_abs) ==
                  PG_OK);
        TU_EXPECT(pg_measure_slice_normalized(&m, 0.2f, 0.8f, &w_norm) ==
                  PG_OK);
        TU_EXPECT(rec_abs.count == rec_norm.count);
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
        rec_t failing;

        memset(&failing, 0, sizeof(failing));
        failing.fail_fast = true;
        TU_EXPECT(pg_measure_slice(&m, 0.0f, 5.0f, &writer) == PG_OK);
        writer = rec_writer(&failing);
        TU_EXPECT(pg_measure_slice(&m, 0.0f, 5.0f, &writer) ==
                  PG_ERR_WORKSPACE_TOO_SMALL);
    }

    /* Incomplete writer (missing callback) is rejected up front. */
    {
        pg_path_writer_t incomplete = rec_writer(&rec);

        incomplete.quad_to = NULL;
        TU_EXPECT(pg_measure_slice(&m, 0.0f, 5.0f, &incomplete) ==
                  PG_ERR_INVALID_ARG);
    }

    /* Uninitialized measure is rejected. */
    {
        pg_measure_t zeroed;

        memset(&zeroed, 0, sizeof(zeroed));
        writer = rec_writer(&rec);
        TU_EXPECT(pg_measure_slice(&zeroed, 0.0f, 1.0f, &writer) ==
                  PG_ERR_INVALID_ARG);
    }

    /* Workspace exhaustion through the buffer writer: explicit error and no
     * silently truncated path. */
    {
        pg_cmd_t cmds[1];
        pg_path_buffer_t buffer;
        pg_path_writer_t buffer_writer;
        pg_path_t out;

        pg_path_buffer_init(&buffer, cmds, 1u);
        buffer_writer = pg_path_buffer_writer(&buffer);
        TU_EXPECT(pg_measure_slice(&m, 5.0f, 15.0f, &buffer_writer) ==
                  PG_ERR_WORKSPACE_TOO_SMALL);
        TU_EXPECT(buffer.overflowed && buffer.count == 1u);
        TU_EXPECT(pg_path_buffer_to_path(&buffer, &out) ==
                  PG_ERR_WORKSPACE_TOO_SMALL);
    }

    /* Degenerate source measure (zero total length) is rejected. */
    {
        pg_measure_t degenerate = m;

        degenerate.total_length = 0.0f;
        writer = rec_writer(&rec);
        TU_EXPECT(pg_measure_slice(&degenerate, 0.0f, 1.0f, &writer) ==
                  PG_ERR_DEGENERATE);
        TU_EXPECT(pg_measure_slice_normalized(&degenerate, 0.0f, 1.0f,
                                              &writer) ==
                  PG_ERR_DEGENERATE);
    }

    return TU_SUMMARY() ? 1 : 0;
}
