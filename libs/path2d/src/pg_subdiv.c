/**
 * @file pg_subdiv.c
 * @brief Shared adaptive subdivision engine: flatness test + bisection.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "pg_internal.h"

#include <math.h>

float pg_point_dist(pg_point_t a, pg_point_t b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;

    return sqrtf(dx * dx + dy * dy);
}

float pg_point_line_dist(pg_point_t p, pg_point_t a, pg_point_t b)
{
    float ex = b.x - a.x;
    float ey = b.y - a.y;
    float wx = p.x - a.x;
    float wy = p.y - a.y;
    float chord = sqrtf(ex * ex + ey * ey);

    if (!(chord > PG_EPSILON)) {
        return sqrtf(wx * wx + wy * wy);
    }
    /* |cross(e, w)| / |e| is the perpendicular distance to the infinite
     * line through a and b. */
    return fabsf(ex * wy - ey * wx) / chord;
}

bool pg_point_is_finite(pg_point_t p)
{
    return isfinite(p.x) && isfinite(p.y);
}

/**
 * Flatness test used by both flattening and arc-length measurement.
 *
 * Two independent conditions must hold before a span may be replaced by its
 * chord:
 *
 * 1. Perpendicular deviation - the standard measure: no control point may
 *    lie farther than `tolerance` from the chord line.
 * 2. Control-polygon excess - |p0p1| + ... + |pn-1pn| - |p0pn| must not
 *    exceed `tolerance`. Perpendicular deviation alone is blind to
 *    collinear overshoot/backtracking curves, e.g. M(0,0) Q(100,0) (10,0):
 *    every control point lies ON the chord, yet the curve overshoots to
 *    x ~ 52.63 before returning to x = 10, so the chord (length 10) is not a
 *    valid approximation (true arc length ~ 95.2632). The excess term is a
 *    conservative bound on in-line deviation and forces subdivision until
 *    the backtracking has been resolved into monotone leaves. Monotone
 *    collinear spans have excess 0 and still flatten in one step.
 *
 * Sources: control-polygon deviation bound (Foley/van Dam flatness test,
 * also used by AGG/Skia style flatteners); SVG renderer tolerance practice.
 */
static bool pg_span_is_flat(const pg_span_t *span, pg_span_kind_t kind,
                            float tolerance)
{
    float chord;
    float polygon;
    float excess;
    float perp;

    if (kind == PG_SPAN_LINE) {
        return true; /* straight by construction */
    }
    chord = pg_point_dist(span->p0, span->p3);
    polygon = pg_point_dist(span->p0, span->p1);
    perp = pg_point_line_dist(span->p1, span->p0, span->p3);
    if (kind == PG_SPAN_CUBIC) {
        float p2_perp;

        polygon += pg_point_dist(span->p1, span->p2);
        polygon += pg_point_dist(span->p2, span->p3);
        p2_perp = pg_point_line_dist(span->p2, span->p0, span->p3);
        if (p2_perp > perp) {
            perp = p2_perp;
        }
    }
    else { /* PG_SPAN_QUAD: p2 mirrors the end point */
        polygon += pg_point_dist(span->p1, span->p2);
    }
    excess = polygon - chord;
    return perp <= tolerance && excess <= tolerance;
}

/**
 * Recursively bisects a span with De Casteljau until it is flat enough (or
 * the PG_MAX_RECURSION bound is reached) and forwards every leaf span.
 */
static pg_result_t pg_subdiv_emit(const pg_span_t *span, pg_span_kind_t kind,
                                  float tolerance, pg_span_fn on_span,
                                  void *ctx, uint16_t command_index,
                                  unsigned depth)
{
    pg_result_t res;
    float t_mid;

    if (pg_span_is_flat(span, kind, tolerance) || depth >= PG_MAX_RECURSION) {
        return on_span(ctx, span, kind, command_index);
    }
    t_mid = (span->t0 + span->t1) * 0.5f;
    if (kind == PG_SPAN_QUAD) {
        pg_quad_t left;
        pg_quad_t right;
        pg_span_t left_span;
        pg_span_t right_span;

        pg_quad_split(span->p0, span->p1, span->p3, 0.5f, &left, &right);
        left_span = (pg_span_t){ left.p0, left.p1, left.p2, left.p2,
                                 span->t0, t_mid };
        right_span = (pg_span_t){ right.p0, right.p1, right.p2, right.p2,
                                  t_mid, span->t1 };
        res = pg_subdiv_emit(&left_span, kind, tolerance, on_span, ctx,
                             command_index, depth + 1u);
        if (res != PG_OK) {
            return res;
        }
        return pg_subdiv_emit(&right_span, kind, tolerance, on_span, ctx,
                              command_index, depth + 1u);
    }
    {
        pg_cubic_t left;
        pg_cubic_t right;
        pg_span_t left_span;
        pg_span_t right_span;

        pg_cubic_split(span->p0, span->p1, span->p2, span->p3, 0.5f, &left,
                       &right);
        left_span = (pg_span_t){ left.p0, left.p1, left.p2, left.p3,
                                 span->t0, t_mid };
        right_span = (pg_span_t){ right.p0, right.p1, right.p2, right.p3,
                                  t_mid, span->t1 };
        res = pg_subdiv_emit(&left_span, kind, tolerance, on_span, ctx,
                             command_index, depth + 1u);
        if (res != PG_OK) {
            return res;
        }
        return pg_subdiv_emit(&right_span, kind, tolerance, on_span, ctx,
                              command_index, depth + 1u);
    }
}

pg_result_t pg_path_walk(const pg_path_t *path, float tolerance,
                         pg_move_fn on_move, pg_span_fn on_span, void *ctx)
{
    pg_point_t cursor = { 0.0f, 0.0f };
    pg_point_t start = { 0.0f, 0.0f };
    int have_cursor = 0;
    pg_result_t res;
    uint16_t i;

    if (path == NULL || on_span == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    if (!(tolerance > 0.0f)) {
        return PG_ERR_INVALID_ARG;
    }
    if (tolerance < PG_MIN_TOLERANCE) {
        tolerance = PG_MIN_TOLERANCE;
    }
    res = pg_path_validate(path);
    if (res != PG_OK) {
        return res;
    }

    for (i = 0; i < path->cmd_count; i++) {
        const pg_cmd_t *cmd = &path->cmds[i];
        pg_span_t span;

        if (cmd->type == PG_CMD_MOVE) {
            cursor = cmd->p1;
            start = cmd->p1;
            have_cursor = 1;
            if (on_move != NULL) {
                res = on_move(ctx, cursor);
                if (res != PG_OK) {
                    return res;
                }
            }
            continue;
        }
        /* pg_path_validate() guarantees the first command is MOVE. */
        if (!have_cursor) {
            return PG_ERR_INVALID_PATH;
        }
        switch (cmd->type) {
        case PG_CMD_LINE:
            span.p1 = cmd->p1;
            span.p2 = cmd->p1;
            span.p3 = cmd->p1;
            break;
        case PG_CMD_QUAD:
            span.p1 = cmd->p1;
            span.p2 = cmd->p2;
            span.p3 = cmd->p2;
            break;
        case PG_CMD_CUBIC:
            span.p1 = cmd->p1;
            span.p2 = cmd->p2;
            span.p3 = cmd->p3;
            break;
        case PG_CMD_CLOSE:
            span.p1 = start;
            span.p2 = start;
            span.p3 = start;
            break;
        default:
            return PG_ERR_INVALID_PATH; /* unreachable after validation */
        }
        span.p0 = cursor;
        span.t0 = 0.0f;
        span.t1 = 1.0f;
        res = pg_subdiv_emit(&span, cmd->type == PG_CMD_CUBIC ? PG_SPAN_CUBIC
                                : cmd->type == PG_CMD_QUAD ? PG_SPAN_QUAD
                                                           : PG_SPAN_LINE,
                             tolerance, on_span, ctx, i, 0u);
        if (res != PG_OK) {
            return res;
        }
        cursor = span.p3;
    }
    return PG_OK;
}
