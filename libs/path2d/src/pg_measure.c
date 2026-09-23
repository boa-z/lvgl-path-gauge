/* SPDX-License-Identifier: MIT */
#include "path2d/pg_measure.h"
#include "path2d/pg_path.h"
#include "path2d/pg_bezier.h"

#include <math.h>

typedef struct {
    pg_measure_t *measure;
    float dist;
    float tolerance;
    pg_result_t err;
} pg_build_t;

static float pg_pt_dist(pg_point_t a, pg_point_t b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return sqrtf(dx * dx + dy * dy);
}

static float pg_line_dist(pg_point_t p, pg_point_t a, pg_point_t b)
{
    float ex = b.x - a.x;
    float ey = b.y - a.y;
    float wx = p.x - a.x;
    float wy = p.y - a.y;
    float chord = sqrtf(ex * ex + ey * ey);
    float cross;

    if (!(chord > PG_EPSILON)) {
        return sqrtf(wx * wx + wy * wy);
    }
    cross = ex * wy - ey * wx;
    return fabsf(cross) / chord;
}

static float pg_clamp01(float t)
{
    if (!(t > 0.0f)) {
        return 0.0f;
    }
    if (!(t < 1.0f)) {
        return 1.0f;
    }
    return t;
}

static void pg_push(pg_build_t *b, float dist, float t, uint16_t cmd)
{
    pg_measure_sample_t *s;

    if (b->err != PG_OK) {
        return;
    }
    if (b->measure->sample_count >= b->measure->sample_capacity) {
        b->err = PG_ERR_WORKSPACE_TOO_SMALL;
        return;
    }
    s = &b->measure->samples[b->measure->sample_count];
    s->distance = dist;
    s->t = t;
    s->command_index = cmd;
    b->measure->sample_count++;
}

static void pg_quad_build(pg_build_t *b, pg_point_t p0, pg_point_t p1,
                          pg_point_t p2, float t0, float t1, uint16_t cmd,
                          unsigned depth)
{
    float chord;

    if (b->err != PG_OK) {
        return;
    }
    if (pg_line_dist(p1, p0, p2) <= b->tolerance || depth >= PG_MAX_RECURSION) {
        chord = pg_pt_dist(p0, p2);
        if (chord > PG_EPSILON) {
            b->dist += chord;
            pg_push(b, b->dist, t1, cmd);
        }
        return;
    }
    {
        pg_quad_t left;
        pg_quad_t right;
        float tm = (t0 + t1) * 0.5f;

        pg_quad_split(p0, p1, p2, 0.5f, &left, &right);
        pg_quad_build(b, left.p0, left.p1, left.p2, t0, tm, cmd, depth + 1u);
        pg_quad_build(b, right.p0, right.p1, right.p2, tm, t1, cmd, depth + 1u);
    }
}

static void pg_cubic_build(pg_build_t *b, pg_point_t p0, pg_point_t p1,
                           pg_point_t p2, pg_point_t p3, float t0, float t1,
                           uint16_t cmd, unsigned depth)
{
    float d1 = pg_line_dist(p1, p0, p3);
    float d2 = pg_line_dist(p2, p0, p3);
    float flat = d1 > d2 ? d1 : d2;
    float chord;

    if (b->err != PG_OK) {
        return;
    }
    if (flat <= b->tolerance || depth >= PG_MAX_RECURSION) {
        chord = pg_pt_dist(p0, p3);
        if (chord > PG_EPSILON) {
            b->dist += chord;
            pg_push(b, b->dist, t1, cmd);
        }
        return;
    }
    {
        pg_cubic_t left;
        pg_cubic_t right;
        float tm = (t0 + t1) * 0.5f;

        pg_cubic_split(p0, p1, p2, p3, 0.5f, &left, &right);
        pg_cubic_build(b, left.p0, left.p1, left.p2, left.p3, t0, tm, cmd,
                       depth + 1u);
        pg_cubic_build(b, right.p0, right.p1, right.p2, right.p3, tm, t1, cmd,
                       depth + 1u);
    }
}

pg_result_t pg_measure_init(pg_measure_t *measure, const pg_path_t *path,
                            pg_measure_sample_t *workspace, uint16_t workspace_count,
                            float tolerance)
{
    pg_build_t b;
    pg_point_t cur = { 0.0f, 0.0f };
    pg_point_t start = { 0.0f, 0.0f };
    pg_point_t end;
    int have = 0;
    int found = 0;
    pg_result_t ok;
    uint16_t i;
    float chord;

    if (measure == NULL || path == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    if (workspace == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    if (workspace_count < PG_MEASURE_MIN_SAMPLES) {
        return PG_ERR_WORKSPACE_TOO_SMALL;
    }
    if (!(tolerance > 0.0f)) {
        return PG_ERR_INVALID_ARG;
    }
    if (tolerance < PG_MIN_TOLERANCE) {
        tolerance = PG_MIN_TOLERANCE;
    }
    ok = pg_path_validate(path);
    if (ok != PG_OK) {
        return ok;
    }

    measure->path = path;
    measure->samples = workspace;
    measure->sample_count = 0;
    measure->sample_capacity = workspace_count;
    measure->total_length = 0.0f;

    b.measure = measure;
    b.dist = 0.0f;
    b.tolerance = tolerance;
    b.err = PG_OK;

    for (i = 0; i < path->cmd_count; i++) {
        const pg_cmd_t *c = &path->cmds[i];

        if (c->type == PG_CMD_MOVE) {
            cur = c->p1;
            start = c->p1;
            have = 1;
            continue;
        }
        if (!have) {
            return PG_ERR_INVALID_PATH; /* unreachable after validate */
        }
        if (!found) {
            pg_push(&b, 0.0f, 0.0f, i);
            if (b.err != PG_OK) {
                return b.err; /* unreachable: capacity >= 2 */
            }
            found = 1;
        }
        if (c->type == PG_CMD_LINE) {
            end = c->p1;
            chord = pg_pt_dist(cur, end);
            if (chord > PG_EPSILON) {
                b.dist += chord;
                pg_push(&b, b.dist, 1.0f, i);
            }
            cur = end;
        }
        else if (c->type == PG_CMD_CLOSE) {
            end = start;
            chord = pg_pt_dist(cur, end);
            if (chord > PG_EPSILON) {
                b.dist += chord;
                pg_push(&b, b.dist, 1.0f, i);
            }
            cur = end;
        }
        else if (c->type == PG_CMD_QUAD) {
            pg_quad_build(&b, cur, c->p1, c->p2, 0.0f, 1.0f, i, 0u);
            cur = c->p2;
        }
        else {
            pg_cubic_build(&b, cur, c->p1, c->p2, c->p3, 0.0f, 1.0f, i, 0u);
            cur = c->p3;
        }
        if (b.err != PG_OK) {
            return b.err;
        }
    }

    if (!found) {
        return PG_ERR_DEGENERATE; /* MOVE-only path */
    }
    if (!(b.dist > PG_EPSILON)) {
        return PG_ERR_DEGENERATE; /* all segments zero-length */
    }
    measure->total_length = b.dist;
    return PG_OK;
}

float pg_measure_get_length(const pg_measure_t *measure)
{
    if (measure == NULL) {
        return 0.0f;
    }
    return measure->total_length;
}

/* Resolve the absolute start/end points of command idx (a draw/CLOSE
 * command). Validation guarantees cmds[0] is MOVE, so `have` is set. */
static void pg_cmd_span(const pg_path_t *path, uint16_t idx, pg_point_t *p0,
                        pg_point_t *p1end, pg_cmd_t *cmd)
{
    pg_point_t cur = { 0.0f, 0.0f };
    pg_point_t start = { 0.0f, 0.0f };
    uint16_t i;

    *p0 = path->cmds[0].p1;
    *p1end = path->cmds[0].p1;
    *cmd = path->cmds[0];
    for (i = 0; i < path->cmd_count; i++) {
        const pg_cmd_t *c = &path->cmds[i];

        if (c->type == PG_CMD_MOVE) {
            cur = c->p1;
            start = c->p1;
        }
        else {
            pg_point_t end;

            if (c->type == PG_CMD_LINE) {
                end = c->p1;
            }
            else if (c->type == PG_CMD_QUAD) {
                end = c->p2;
            }
            else if (c->type == PG_CMD_CUBIC) {
                end = c->p3;
            }
            else {
                end = start;
            }
            if (i == idx) {
                *p0 = cur;
                *p1end = end;
                *cmd = *c;
                return;
            }
            cur = end;
        }
    }
}

static pg_point_t pg_eval_cmd(const pg_cmd_t *cmd, pg_point_t p0,
                              pg_point_t pend, float t)
{
    pg_point_t out;

    switch (cmd->type) {
    case PG_CMD_QUAD:
        return pg_quad_eval(p0, cmd->p1, cmd->p2, t);
    case PG_CMD_CUBIC:
        return pg_cubic_eval(p0, cmd->p1, cmd->p2, cmd->p3, t);
    default: /* LINE and CLOSE are straight spans */
        out.x = p0.x + (pend.x - p0.x) * t;
        out.y = p0.y + (pend.y - p0.y) * t;
        return out;
    }
}

static pg_point_t pg_deriv_cmd(const pg_cmd_t *cmd, pg_point_t p0,
                               pg_point_t pend, float t)
{
    pg_point_t out;

    switch (cmd->type) {
    case PG_CMD_QUAD:
        return pg_quad_derivative(p0, cmd->p1, cmd->p2, t);
    case PG_CMD_CUBIC:
        return pg_cubic_derivative(p0, cmd->p1, cmd->p2, cmd->p3, t);
    default:
        out.x = pend.x - p0.x;
        out.y = pend.y - p0.y;
        return out;
    }
}

pg_point_t pg_vec_normalize(pg_point_t v)
{
    float n = sqrtf(v.x * v.x + v.y * v.y);
    pg_point_t out;

    if (!(n > PG_EPSILON)) {
        out.x = 1.0f;
        out.y = 0.0f;
        return out;
    }
    out.x = v.x / n;
    out.y = v.y / n;
    return out;
}

pg_point_t pg_tangent_to_normal(pg_point_t tangent)
{
    pg_point_t out;

    out.x = -tangent.y;
    out.y = tangent.x;
    return out;
}

/* Evaluate position + robust unit tangent for command span at t. */
static void pg_span_pos_tan(const pg_path_t *path, uint16_t cmd_idx, float t,
                            pg_point_t *position, pg_point_t *tangent)
{
    pg_point_t p0;
    pg_point_t pend;
    pg_cmd_t cmd;
    pg_point_t d;
    float n;

    pg_cmd_span(path, cmd_idx, &p0, &pend, &cmd);
    t = pg_clamp01(t);
    *position = pg_eval_cmd(&cmd, p0, pend, t);
    d = pg_deriv_cmd(&cmd, p0, pend, t);
    n = sqrtf(d.x * d.x + d.y * d.y);
    if (n > PG_EPSILON) {
        tangent->x = d.x / n;
        tangent->y = d.y / n;
        return;
    }
    d.x = pend.x - p0.x;
    d.y = pend.y - p0.y;
    *tangent = pg_vec_normalize(d);
}

pg_result_t pg_measure_get_pos_tan(const pg_measure_t *measure, float distance,
                                   pg_point_t *position, pg_point_t *tangent)
{
    const pg_measure_sample_t *s;
    uint16_t lo;
    uint16_t hi;
    uint16_t mid;
    float f;
    float dd;
    float t;

    if (measure == NULL || position == NULL || tangent == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    if (measure->samples == NULL || measure->path == NULL ||
        measure->sample_count < PG_MEASURE_MIN_SAMPLES) {
        return PG_ERR_INVALID_ARG;
    }
    if (!(measure->total_length > PG_EPSILON)) {
        return PG_ERR_DEGENERATE;
    }
    if (distance != distance) { /* NaN */
        return PG_ERR_INVALID_ARG;
    }
    if (distance <= 0.0f) {
        /* t = 0 of the first draw command is exactly the path start
         * (current point after the leading MOVEs). */
        pg_span_pos_tan(measure->path, measure->samples[0].command_index, 0.0f,
                        position, tangent);
        return PG_OK;
    }
    if (distance >= measure->total_length) {
        s = &measure->samples[measure->sample_count - 1u];
        pg_span_pos_tan(measure->path, s->command_index, s->t, position, tangent);
        return PG_OK;
    }

    s = measure->samples;
    lo = 0;
    hi = (uint16_t)(measure->sample_count - 1u);
    while ((uint16_t)(hi - lo) > 1u) {
        mid = (uint16_t)(lo + (hi - lo) / 2u);
        if (s[mid].distance <= distance) {
            lo = mid;
        }
        else {
            hi = mid;
        }
    }
    dd = s[hi].distance - s[lo].distance;
    f = (!(dd > 0.0f)) ? 1.0f : (distance - s[lo].distance) / dd;
    if (s[lo].command_index == s[hi].command_index) {
        t = s[lo].t + (s[hi].t - s[lo].t) * f;
    }
    else {
        /* Boundary pair: lo ends the old command, hi starts a new one whose
         * local span always begins at t = 0. */
        t = s[hi].t * f;
    }
    pg_span_pos_tan(measure->path, s[hi].command_index, t, position, tangent);
    return PG_OK;
}

pg_result_t pg_measure_get_pos_tan_normalized(const pg_measure_t *measure,
                                              float normalized,
                                              pg_point_t *position,
                                              pg_point_t *tangent)
{
    if (measure == NULL || position == NULL || tangent == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    if (normalized != normalized) { /* NaN */
        return PG_ERR_INVALID_ARG;
    }
    if (normalized <= 0.0f) {
        return pg_measure_get_pos_tan(measure, 0.0f, position, tangent);
    }
    if (normalized >= 1.0f) {
        return pg_measure_get_pos_tan(measure, measure->total_length, position,
                                      tangent);
    }
    return pg_measure_get_pos_tan(measure, normalized * measure->total_length,
                                  position, tangent);
}
