/**
 * @file pg_flatten.c
 * @brief Adaptive flattening into move_to/line_to writer calls.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "path2d/pg_flatten.h"

#include "pg_internal.h"

typedef struct {
    const pg_path_writer_t *writer; /**< Caller sink (borrowed). */
} pg_flat_t;

static pg_result_t pg_flat_move(void *ctx, pg_point_t to)
{
    pg_flat_t *flat = ctx;

    return flat->writer->move_to(flat->writer->ctx, to);
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
    if (pg_point_dist(span->p0, span->p3) <= PG_EPSILON) {
        return PG_OK;
    }
    return flat->writer->line_to(flat->writer->ctx, span->p3);
}

pg_result_t pg_path_flatten(const pg_path_t *path, float tolerance,
                            const pg_path_writer_t *writer)
{
    pg_flat_t flat;

    if (writer == NULL || writer->move_to == NULL || writer->line_to == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    flat.writer = writer;
    return pg_path_walk(path, tolerance, pg_flat_move, pg_flat_leaf, &flat);
}
