/**
 * @file pg_flatten.c
 * @brief Adaptive flattening of paths into a vertex stream.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "path2d/pg_flatten.h"

#include "pg_internal.h"

typedef struct {
    pg_flatten_cb cb; /**< Vertex sink from the caller. */
    void *ctx;        /**< Caller context forwarded to the sink. */
} pg_flat_t;

static pg_result_t pg_flat_move(void *ctx, pg_point_t to)
{
    pg_flat_t *flat = ctx;

    flat->cb(flat->ctx, to);
    return PG_OK;
}

/* Flat leaves are emitted as their chord end point. Truly degenerate leaves
 * (zero chord) carry no geometry and are skipped; loops and collinear
 * backtracking spans were already subdivided by the shared engine. */
static pg_result_t pg_flat_leaf(void *ctx, const pg_span_t *span,
                                pg_span_kind_t kind, uint16_t command_index)
{
    pg_flat_t *flat = ctx;

    (void)kind;
    (void)command_index;
    if (pg_point_dist(span->p0, span->p3) > PG_EPSILON) {
        flat->cb(flat->ctx, span->p3);
    }
    return PG_OK;
}

pg_result_t pg_path_flatten(const pg_path_t *path, float tolerance,
                            pg_flatten_cb cb, void *ctx)
{
    pg_flat_t flat;

    if (cb == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    flat.cb = cb;
    flat.ctx = ctx;
    return pg_path_walk(path, tolerance, pg_flat_move, pg_flat_leaf, &flat);
}
