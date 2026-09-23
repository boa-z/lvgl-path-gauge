/**
 * @file pg_slice.c
 * @brief Arc-length range extraction (slicing) into a path writer.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "path2d/pg_measure.h"
#include "path2d/pg_writer.h"

#include <math.h>

#include "pg_internal.h"

/**
 * Restricts a quadratic Bezier to the parameter range [t0, t1].
 *
 * Two De Casteljau splits keep the result an exact quadratic Bezier (never a
 * polyline approximation): first cut away [t1, 1], then cut away the part of
 * the remainder before t0.
 */
static void pg_quad_range(pg_quad_t quad, float t0, float t1, pg_quad_t *out)
{
    pg_quad_t head;
    float u;

    if (t1 < 1.0f) {
        pg_quad_split(quad.p0, quad.p1, quad.p2, t1, &head, NULL);
    }
    else {
        head = quad;
    }
    u = (t1 > PG_EPSILON) ? (t0 / t1) : 0.0f;
    if (u > 0.0f) {
        pg_quad_split(head.p0, head.p1, head.p2, u, NULL, out);
    }
    else {
        *out = head;
    }
}

/** Cubic counterpart of pg_quad_range(). */
static void pg_cubic_range(pg_cubic_t cubic, float t0, float t1,
                           pg_cubic_t *out)
{
    pg_cubic_t head;
    float u;

    if (t1 < 1.0f) {
        pg_cubic_split(cubic.p0, cubic.p1, cubic.p2, cubic.p3, t1, &head,
                       NULL);
    }
    else {
        head = cubic;
    }
    u = (t1 > PG_EPSILON) ? (t0 / t1) : 0.0f;
    if (u > 0.0f) {
        pg_cubic_split(head.p0, head.p1, head.p2, head.p3, u, NULL, out);
    }
    else {
        *out = head;
    }
}

/**
 * Emits the geometry of command `index` restricted to local [t0, t1].
 *
 * Curves keep their original degree through De Casteljau range extraction;
 * LINE/CLOSE pieces collapse to a single line_to (the start point is already
 * the current writer position) and are skipped when they carry no length.
 */
static pg_result_t pg_emit_piece(const pg_path_t *path, uint16_t index,
                                 float t0, float t1,
                                 const pg_path_writer_t *writer)
{
    pg_point_t p0;
    pg_point_t end;
    pg_cmd_t cmd;
    pg_point_t piece_start;
    pg_point_t piece_end;

    if (!(t1 > t0)) {
        return PG_OK; /* zero-width piece */
    }
    pg_cmd_span(path, index, &p0, &end, &cmd);
    if (cmd.type == PG_CMD_QUAD) {
        pg_quad_t quad = { p0, cmd.p1, cmd.p2 };
        pg_quad_t sub;

        pg_quad_range(quad, t0, t1, &sub);
        return writer->quad_to(writer->ctx, sub.p1, sub.p2);
    }
    if (cmd.type == PG_CMD_CUBIC) {
        pg_cubic_t cubic = { p0, cmd.p1, cmd.p2, cmd.p3 };
        pg_cubic_t sub;

        pg_cubic_range(cubic, t0, t1, &sub);
        return writer->cubic_to(writer->ctx, sub.p1, sub.p2, sub.p3);
    }
    piece_start = pg_cmd_eval(&cmd, p0, end, t0);
    piece_end = pg_cmd_eval(&cmd, p0, end, t1);
    if (pg_point_dist(piece_start, piece_end) <= PG_EPSILON) {
        return PG_OK; /* zero-length straight piece (e.g. CLOSE to itself) */
    }
    return writer->line_to(writer->ctx, piece_end);
}

pg_result_t pg_measure_slice(const pg_measure_t *measure, float start,
                             float end, const pg_path_writer_t *writer)
{
    uint16_t first_cmd;
    uint16_t last_cmd;
    float first_t;
    float last_t;
    uint16_t index;
    bool have_previous = false;
    pg_point_t previous_end = { 0.0f, 0.0f };
    pg_result_t res;

    if (measure == NULL || writer == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    if (writer->move_to == NULL || writer->line_to == NULL ||
        writer->quad_to == NULL || writer->cubic_to == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    if (measure->samples == NULL || measure->path == NULL ||
        measure->sample_count < PG_MEASURE_MIN_SAMPLES) {
        return PG_ERR_INVALID_ARG;
    }
    if (!(measure->total_length > PG_EPSILON)) {
        return PG_ERR_DEGENERATE;
    }
    if (isnan(start) || isnan(end)) {
        return PG_ERR_INVALID_ARG;
    }
    if (start > end) {
        return PG_ERR_INVALID_ARG;
    }
    if (start < 0.0f) {
        start = 0.0f;
    }
    if (end > measure->total_length) {
        end = measure->total_length;
    }

    pg_measure_locate(measure, start, &first_cmd, &first_t);
    pg_measure_locate(measure, end, &last_cmd, &last_t);

    {
        pg_point_t p0;
        pg_point_t cmd_end;
        pg_cmd_t cmd;

        pg_cmd_span(measure->path, first_cmd, &p0, &cmd_end, &cmd);
        res = writer->move_to(writer->ctx,
                              pg_cmd_eval(&cmd, p0, cmd_end, first_t));
        if (res != PG_OK) {
            return res;
        }
    }
    if (start >= end) {
        return PG_OK; /* zero-length slice: a single MOVE */
    }

    for (index = first_cmd;; index++) {
        float t0;
        float t1;
        pg_point_t p0;
        pg_point_t cmd_end;
        pg_cmd_t cmd;
        pg_point_t piece_start;

        if (measure->path->cmds[index].type == PG_CMD_MOVE) {
            /* MOVEs carry no geometry and never own LUT samples; the subpath
             * break is detected from the position jump of the next command. */
            if (index == last_cmd) {
                break;
            }
            continue;
        }
        t0 = (index == first_cmd) ? first_t : 0.0f;
        t1 = (index == last_cmd) ? last_t : 1.0f;
        pg_cmd_span(measure->path, index, &p0, &cmd_end, &cmd);
        piece_start = pg_cmd_eval(&cmd, p0, cmd_end, t0);
        if (have_previous &&
            pg_point_dist(previous_end, piece_start) > PG_EPSILON) {
            /* Multi-subpath break: the next subpath starts elsewhere, so the
             * writer must open a new contour instead of drawing a jump. */
            res = writer->move_to(writer->ctx, piece_start);
            if (res != PG_OK) {
                return res;
            }
        }
        have_previous = true;
        res = pg_emit_piece(measure->path, index, t0, t1, writer);
        if (res != PG_OK) {
            return res;
        }
        previous_end = pg_cmd_eval(&cmd, p0, cmd_end, t1);
        if (index == last_cmd) {
            break;
        }
    }
    return PG_OK;
}

pg_result_t pg_measure_slice_normalized(const pg_measure_t *measure,
                                        float start, float end,
                                        const pg_path_writer_t *writer)
{
    if (measure == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    if (isnan(start) || isnan(end)) {
        return PG_ERR_INVALID_ARG;
    }
    if (start > end) {
        return PG_ERR_INVALID_ARG;
    }
    if (!(measure->total_length > PG_EPSILON)) {
        return PG_ERR_DEGENERATE;
    }
    if (start < 0.0f) {
        start = 0.0f;
    }
    if (start > 1.0f) {
        start = 1.0f;
    }
    if (end < 0.0f) {
        end = 0.0f;
    }
    if (end > 1.0f) {
        end = 1.0f;
    }
    return pg_measure_slice(measure, start * measure->total_length,
                            end * measure->total_length, writer);
}
