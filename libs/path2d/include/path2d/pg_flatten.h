/**
 * @file pg_flatten.h
 * @brief Adaptive subdivision flattening into move_to/line_to writer calls.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#ifndef PATH2D_FLATTEN_H
#define PATH2D_FLATTEN_H

#include "path2d/pg_types.h"
#include "path2d/pg_writer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Flattens a path into a move_to/line_to writer stream.
 *
 * Uses the shared subdivision engine also used by pg_measure_init(): a span
 * is accepted only when (a) no control point deviates more than `tolerance`
 * from the chord AND (b) the control-polygon length exceeds the chord by no
 * more than `tolerance`. The second condition forces subdivision of collinear
 * overshoot/backtracking curves (e.g. M(0,0) Q(100,0) (10,0)), which the
 * perpendicular test alone would flatten into a single wrong chord. Monotone
 * collinear spans still emit one chord. Truly degenerate leaves (zero chord
 * after subdivision) emit nothing.
 *
 * Output contract: one move_to per MOVE command (so contour boundaries
 * survive into the sink) followed by one line_to per flat leaf. quad_to and
 * cubic_to are never called; renderers must not consume a bare point stream
 * because it cannot express contour breaks.
 *
 * @param[in] path       Path to flatten (validated, finite coordinates).
 * @param[in] tolerance  Flatness tolerance in path coordinate units;
 *                       values below PG_MIN_TOLERANCE are clamped up.
 * @param[in] writer     Sink requiring non-NULL move_to and line_to.
 *                       Sink errors abort the walk immediately and are
 *                       propagated (e.g. PG_ERR_WORKSPACE_TOO_SMALL).
 * @return               PG_OK on success; PG_ERR_INVALID_ARG for NULL
 *                       path/writer/missing callbacks or a
 *                       non-finite/non-positive tolerance;
 *                       PG_ERR_INVALID_PATH for malformed paths;
 *                       otherwise the first error from the sink.
 *
 * @note No heap is used; the writer callbacks run synchronously.
 */
pg_result_t pg_path_flatten(const pg_path_t *path, float tolerance,
                            const pg_path_writer_t *writer);

#ifdef __cplusplus
}
#endif

#endif /* PATH2D_FLATTEN_H */
