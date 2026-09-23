/**
 * @file pg_internal.h
 * @brief Internal cross-translation-unit helpers. Not installed.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#ifndef PATH2D_INTERNAL_H
#define PATH2D_INTERNAL_H

#include "path2d/pg_bezier.h"
#include "path2d/pg_measure.h"
#include "path2d/pg_path.h"
#include "path2d/pg_types.h"

/** Curve kinds handled by the shared subdivision engine. */
typedef enum {
    PG_SPAN_LINE = 0, /**< Straight span (controls duplicate the end point). */
    PG_SPAN_QUAD,     /**< Quadratic Bezier span (p1 = control). */
    PG_SPAN_CUBIC     /**< Cubic Bezier span (p1/p2 = controls). */
} pg_span_kind_t;

/**
 * One span of a curve over the parameter interval [t0, t1].
 *
 * Point convention: p3 always carries the span end point; p1/p2 mirror it for
 * PG_SPAN_LINE and duplicate p2 for PG_SPAN_QUAD so `end == p3` holds for
 * every kind. Leaf spans of one command share the command's local [0, 1]
 * parameter space, which makes t directly usable by the arc-length LUT.
 */
typedef struct {
    pg_point_t p0; /**< Span start (cursor before the command). */
    pg_point_t p1; /**< First control / end for straight spans. */
    pg_point_t p2; /**< Second control / end for quadratic spans. */
    pg_point_t p3; /**< Span end (always the end point). */
    float t0;      /**< Local parameter at p0, within the owning command. */
    float t1;      /**< Local parameter at p3, within the owning command. */
} pg_span_t;

/**
 * Leaf sink: called for every span considered flat enough (or when the
 * recursion bound is reached).
 *
 * @param[in] ctx            Caller context from pg_path_walk().
 * @param[in] span           Leaf span, valid only during the call.
 * @param[in] kind           Curve kind of the leaf.
 * @param[in] command_index  Index of the owning command in path->cmds.
 * @return                   PG_OK to continue; any other code aborts the walk
 *                           and is propagated to the pg_path_walk() caller.
 */
typedef pg_result_t (*pg_span_fn)(void *ctx, const pg_span_t *span,
                                  pg_span_kind_t kind, uint16_t command_index);

/**
 * Subpath-start sink: called for every MOVE command.
 *
 * @param[in] ctx  Caller context from pg_path_walk().
 * @param[in] to   MOVE target point (new cursor and subpath start).
 * @return         PG_OK to continue; any other code aborts the walk.
 */
typedef pg_result_t (*pg_move_fn)(void *ctx, pg_point_t to);

/**
 * @brief Walks a path with the shared adaptive subdivision engine.
 *
 * Validates the path, clamps the tolerance and iterates the commands:
 * MOVE updates the cursor/subpath start and notifies on_move; LINE, QUAD,
 * CUBIC and CLOSE are subdivided by pg_subdiv_emit() and every flat leaf is
 * delivered to on_span with its owning command index.
 *
 * @param[in] path       Path to walk (validated, finite coordinates required).
 * @param[in] tolerance  Flatness tolerance in path units (clamped to
 *                       >= PG_MIN_TOLERANCE).
 * @param[in] on_move    Optional subpath-start sink (may be NULL).
 * @param[in] on_span    Leaf sink. Cannot be NULL.
 * @param[in] ctx        Opaque pointer forwarded to both sinks.
 * @return               PG_OK on success; PG_ERR_INVALID_ARG for NULL
 *                       path/sink or non-positive tolerance;
 *                       PG_ERR_INVALID_PATH for malformed paths; otherwise
 *                       the first error returned by a sink.
 */
pg_result_t pg_path_walk(const pg_path_t *path, float tolerance,
                         pg_move_fn on_move, pg_span_fn on_span, void *ctx);

/** @brief Euclidean distance between two points. */
float pg_point_dist(pg_point_t a, pg_point_t b);

/**
 * @brief Perpendicular distance from p to the line a-b.
 *
 * Falls back to |p - a| when the chord is shorter than PG_EPSILON so the
 * result stays meaningful for degenerate chords.
 */
float pg_point_line_dist(pg_point_t p, pg_point_t a, pg_point_t b);

/** @brief True when both coordinates are finite (no NaN/Inf). */
bool pg_point_is_finite(pg_point_t p);

/** @brief True when every coordinate consumed by the opcode is finite. */
bool pg_cmd_coords_finite(const pg_cmd_t *cmd);

/**
 * @brief Resolves the geometry of the drawable command at index.
 *
 * @param[in]  path  Path that owns the command.
 * @param[in]  index Command index (must be a drawable command).
 * @param[out] p0    Cursor position before the command. Cannot be NULL.
 * @param[out] end   Command end point (start point for CLOSE). Cannot be NULL.
 * @param[out] cmd   Copy of the command. Cannot be NULL.
 */
void pg_cmd_span(const pg_path_t *path, uint16_t index, pg_point_t *p0,
                 pg_point_t *end, pg_cmd_t *cmd);

/**
 * @brief Evaluates a command at local parameter t.
 *
 * LINE and CLOSE interpolate the chord; QUAD/CUBIC evaluate the curve.
 */
pg_point_t pg_cmd_eval(const pg_cmd_t *cmd, pg_point_t p0, pg_point_t end,
                       float t);

/**
 * @brief First derivative of a command at local parameter t.
 *
 * LINE and CLOSE return the chord vector; QUAD/CUBIC return the curve
 * derivative (not normalized).
 */
pg_point_t pg_cmd_deriv(const pg_cmd_t *cmd, pg_point_t p0, pg_point_t end,
                        float t);

/**
 * @brief Maps an arc distance to (command index, local parameter).
 *
 * distance is clamped to [0, total_length]. Distances at a contour joint
 * resolve to the later command with t = 0 (documented multi-subpath
 * boundary semantics). O(log N) binary search.
 */
void pg_measure_locate(const pg_measure_t *measure, float distance,
                       uint16_t *command_index, float *t);

#endif /* PATH2D_INTERNAL_H */
