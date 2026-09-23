/**
 * @file test_recorder.h
 * @brief Shared pg_path_writer_t recorder used by the flatten/slice tests.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#ifndef TEST_RECORDER_H
#define TEST_RECORDER_H

#include "test_util.h"

#include "path2d/pg_path.h"
#include "path2d/pg_writer.h"

#include <math.h>

#define TU_REC_MAX 4096u

/** Records writer callbacks as pg_cmd_t entries; can be told to fail. */
typedef struct {
    pg_cmd_t cmds[TU_REC_MAX]; /**< Recorded commands in call order. */
    uint16_t count;            /**< Commands stored. */
    unsigned calls;            /**< Callback invocations (including failures). */
    pg_result_t fail_with;     /**< PG_OK to succeed; otherwise returned as-is. */
} tu_rec_t;

static inline void tu_rec_reset(tu_rec_t *rec)
{
    rec->count = 0;
    rec->calls = 0;
    rec->fail_with = PG_OK;
}

static inline pg_result_t tu_rec_append(tu_rec_t *rec, const pg_cmd_t *cmd)
{
    rec->calls++;
    if (rec->fail_with != PG_OK) {
        return rec->fail_with;
    }
    if (cmd->type != PG_CMD_MOVE && rec->count == 0u) {
        return PG_ERR_INVALID_ARG;
    }
    if (rec->count >= TU_REC_MAX) {
        return PG_ERR_WORKSPACE_TOO_SMALL;
    }
    rec->cmds[rec->count] = *cmd;
    rec->count++;
    return PG_OK;
}

static inline pg_result_t tu_rec_move(void *ctx, pg_point_t to)
{
    pg_cmd_t cmd = { PG_CMD_MOVE, to, { 0.0f, 0.0f }, { 0.0f, 0.0f } };

    return tu_rec_append(ctx, &cmd);
}

static inline pg_result_t tu_rec_line(void *ctx, pg_point_t to)
{
    pg_cmd_t cmd = { PG_CMD_LINE, to, { 0.0f, 0.0f }, { 0.0f, 0.0f } };

    return tu_rec_append(ctx, &cmd);
}

static inline pg_result_t tu_rec_quad(void *ctx, pg_point_t control, pg_point_t to)
{
    pg_cmd_t cmd = { PG_CMD_QUAD, control, to, { 0.0f, 0.0f } };

    return tu_rec_append(ctx, &cmd);
}

static inline pg_result_t tu_rec_cubic(void *ctx, pg_point_t c1, pg_point_t c2,
                                       pg_point_t to)
{
    pg_cmd_t cmd = { PG_CMD_CUBIC, c1, c2, to };

    return tu_rec_append(ctx, &cmd);
}

static inline pg_path_writer_t tu_rec_writer(tu_rec_t *rec)
{
    pg_path_writer_t writer = { tu_rec_move, tu_rec_line, tu_rec_quad,
                                tu_rec_cubic, rec };

    return writer;
}

/** Endpoint of a recorded command (endpoint semantics per opcode). */
static inline pg_point_t tu_rec_end(const tu_rec_t *rec, unsigned index)
{
    const pg_cmd_t *cmd = &rec->cmds[index];

    if (cmd->type == PG_CMD_QUAD) {
        return cmd->p2;
    }
    if (cmd->type == PG_CMD_CUBIC) {
        return cmd->p3;
    }
    return cmd->p1;
}

static inline unsigned tu_rec_kind_count(const tu_rec_t *rec, pg_cmd_type_t kind)
{
    unsigned i;
    unsigned n = 0;

    for (i = 0; i < rec->count; i++) {
        if (rec->cmds[i].type == kind) {
            n++;
        }
    }
    return n;
}

/** Polyline length of the recorded MOVE/LINE stream (no heap). */
static inline float tu_rec_length(const tu_rec_t *rec)
{
    pg_point_t cursor = { 0.0f, 0.0f };
    int have = 0;
    float total = 0.0f;
    unsigned i;

    for (i = 0; i < rec->count; i++) {
        pg_point_t end = tu_rec_end(rec, i);

        if (rec->cmds[i].type == PG_CMD_MOVE) {
            cursor = end;
            have = 1;
            continue;
        }
        if (!have) {
            return -1.0f;
        }
        {
            float dx = end.x - cursor.x;
            float dy = end.y - cursor.y;

            total += sqrtf(dx * dx + dy * dy);
        }
        cursor = end;
    }
    return total;
}

static inline float tu_rec_max_x(const tu_rec_t *rec)
{
    float max_x = -1e30f;
    unsigned i;

    for (i = 0; i < rec->count; i++) {
        float x = tu_rec_end(rec, i).x;

        if (x > max_x) {
            max_x = x;
        }
    }
    return max_x;
}

static inline void tu_expect_move(const tu_rec_t *rec, unsigned index, float x,
                                  float y, float eps)
{
    TU_EXPECT(index < rec->count);
    if (index >= rec->count) {
        return;
    }
    TU_EXPECT(rec->cmds[index].type == PG_CMD_MOVE);
    TU_POINT_NEAR(rec->cmds[index].p1, x, y, eps);
}

static inline void tu_expect_line(const tu_rec_t *rec, unsigned index, float x,
                                  float y, float eps)
{
    TU_EXPECT(index < rec->count);
    if (index >= rec->count) {
        return;
    }
    TU_EXPECT(rec->cmds[index].type == PG_CMD_LINE);
    TU_POINT_NEAR(rec->cmds[index].p1, x, y, eps);
}

static inline void tu_expect_quad(const tu_rec_t *rec, unsigned index, float cx,
                                  float cy, float x, float y, float eps)
{
    TU_EXPECT(index < rec->count);
    if (index >= rec->count) {
        return;
    }
    TU_EXPECT(rec->cmds[index].type == PG_CMD_QUAD);
    TU_POINT_NEAR(rec->cmds[index].p1, cx, cy, eps);
    TU_POINT_NEAR(rec->cmds[index].p2, x, y, eps);
}

static inline void tu_expect_cubic(const tu_rec_t *rec, unsigned index,
                                   float c1x, float c1y, float c2x, float c2y,
                                   float x, float y, float eps)
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

#endif /* TEST_RECORDER_H */
