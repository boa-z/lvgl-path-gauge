/**
 * @file pg_flatten.h
 * @brief Adaptive subdivision flattening of paths into polylines.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#ifndef PATH2D_FLATTEN_H
#define PATH2D_FLATTEN_H

#include "path2d/pg_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Receives every emitted flatten vertex.
 *
 * Each subpath start is emitted once, followed by its segment endpoints
 * (curves adaptively subdivided). Runs synchronously inside
 * pg_path_flatten(); must not retain the point.
 *
 * @param[in] ctx    Caller context passed through from pg_path_flatten().
 * @param[in] point  Emitted vertex in path coordinate units.
 */
typedef void (*pg_flatten_cb)(void *ctx, pg_point_t point);

/**
 * @brief Flattens a path into a vertex stream (adaptive subdivision).
 *
 * Flatness estimator: maximum distance from the Bezier control points to
 * the chord (p0-p3); a segment is accepted once it is within tolerance,
 * otherwise it is bisected with De Casteljau (recursion bounded by
 * PG_MAX_RECURSION). Zero-length spans are skipped, never emitted twice.
 *
 * @param[in] path       Path to flatten (validated first).
 * @param[in] tolerance  Flatness tolerance in path coordinate units;
 *                       values below PG_MIN_TOLERANCE are clamped up.
 * @param[in] cb         Vertex sink. Cannot be NULL.
 * @param[in] ctx        Opaque pointer forwarded to every cb call.
 * @return               PG_OK on success, PG_ERR_INVALID_ARG for NULL
 *                       path/callback or non-positive tolerance,
 *                       PG_ERR_INVALID_PATH for malformed paths.
 *
 * @note No heap is used; the callback runs synchronously.
 */
pg_result_t pg_path_flatten(const pg_path_t *path, float tolerance,
                            pg_flatten_cb cb, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* PATH2D_FLATTEN_H */
