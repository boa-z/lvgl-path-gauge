/* SPDX-License-Identifier: MIT */
#include "path2d/pg_flatten.h"
#include "path2d/pg_path.h"
#include "path2d/pg_bezier.h"

#include <math.h>

typedef struct {
    pg_flatten_cb cb;
    void *ctx;
    float tolerance;
} pg_flat_ctx_t;

static float pg_pt_dist(pg_point_t a, pg_point_t b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return sqrtf(dx * dx + dy * dy);
}

/* Distance from p to the line a-b; degrades to |p - a| for tiny chords. */
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

static void pg_quad_flat(pg_flat_ctx_t *f, pg_point_t p0, pg_point_t p1,
                         pg_point_t p2, unsigned depth)
{
    float flat = pg_line_dist(p1, p0, p2);

    if (flat <= f->tolerance || depth >= PG_MAX_RECURSION) {
        if (pg_pt_dist(p0, p2) > PG_EPSILON) {
            f->cb(f->ctx, p2);
        }
        return;
    }
    {
        pg_quad_t left;
        pg_quad_t right;

        pg_quad_split(p0, p1, p2, 0.5f, &left, &right);
        pg_quad_flat(f, left.p0, left.p1, left.p2, depth + 1u);
        pg_quad_flat(f, right.p0, right.p1, right.p2, depth + 1u);
    }
}

static void pg_cubic_flat(pg_flat_ctx_t *f, pg_point_t p0, pg_point_t p1,
                          pg_point_t p2, pg_point_t p3, unsigned depth)
{
    float d1 = pg_line_dist(p1, p0, p3);
    float d2 = pg_line_dist(p2, p0, p3);
    float flat = d1 > d2 ? d1 : d2;

    if (flat <= f->tolerance || depth >= PG_MAX_RECURSION) {
        if (pg_pt_dist(p0, p3) > PG_EPSILON) {
            f->cb(f->ctx, p3);
        }
        return;
    }
    {
        pg_cubic_t left;
        pg_cubic_t right;

        pg_cubic_split(p0, p1, p2, p3, 0.5f, &left, &right);
        pg_cubic_flat(f, left.p0, left.p1, left.p2, left.p3, depth + 1u);
        pg_cubic_flat(f, right.p0, right.p1, right.p2, right.p3, depth + 1u);
    }
}

pg_result_t pg_path_flatten(const pg_path_t *path, float tolerance,
                            pg_flatten_cb cb, void *ctx)
{
    pg_flat_ctx_t f;
    pg_point_t cur = { 0.0f, 0.0f };
    pg_point_t start = { 0.0f, 0.0f };
    pg_point_t end;
    int have = 0;
    pg_result_t ok;
    uint16_t i;

    if (path == NULL || cb == NULL) {
        return PG_ERR_INVALID_ARG;
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

    f.cb = cb;
    f.ctx = ctx;
    f.tolerance = tolerance;

    for (i = 0; i < path->cmd_count; i++) {
        const pg_cmd_t *c = &path->cmds[i];

        switch (c->type) {
        case PG_CMD_MOVE:
            cur = c->p1;
            start = c->p1;
            have = 1;
            cb(ctx, cur);
            break;
        case PG_CMD_LINE:
        case PG_CMD_QUAD:
        case PG_CMD_CUBIC:
        case PG_CMD_CLOSE:
            if (!have) {
                return PG_ERR_INVALID_PATH; /* unreachable after validate */
            }
            if (c->type == PG_CMD_LINE) {
                end = c->p1;
            }
            else if (c->type == PG_CMD_CLOSE) {
                end = start;
            }
            else if (c->type == PG_CMD_QUAD) {
                pg_quad_flat(&f, cur, c->p1, c->p2, 0u);
                cur = c->p2;
                break;
            }
            else {
                pg_cubic_flat(&f, cur, c->p1, c->p2, c->p3, 0u);
                cur = c->p3;
                break;
            }
            if (pg_pt_dist(cur, end) > PG_EPSILON) {
                cb(ctx, end);
            }
            cur = end;
            break;
        default:
            return PG_ERR_INVALID_PATH; /* unreachable after validate */
        }
    }
    return PG_OK;
}
