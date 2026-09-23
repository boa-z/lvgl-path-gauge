/**
 * @file pg_types.h
 * @brief Core types, commands, results and compile-time configuration.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#ifndef PATH2D_TYPES_H
#define PATH2D_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PATH2D_VERSION_MAJOR 0
#define PATH2D_VERSION_MINOR 2
#define PATH2D_VERSION_PATCH 0

/** Scalar type used by all geometry (v1 is float-only by design). */
typedef float pg_float_t;

/** Smallest magnitude treated as nonzero in geometric predicates. */
#define PG_EPSILON 1e-6f

/** Hard cap for adaptive subdivision recursion (stack safety). */
#ifndef PG_MAX_RECURSION
#define PG_MAX_RECURSION 12
#endif

/** Flatten tolerances below this are clamped up to bound subdivision work. */
#define PG_MIN_TOLERANCE 1e-4f

/** Operation outcome codes. */
typedef enum {
    PG_OK = 0,                 /**< Success. */
    PG_ERR_INVALID_ARG,        /**< NULL pointer, NaN/domain error or misuse. */
    PG_ERR_INVALID_PATH,       /**< Structurally invalid or non-finite path. */
    PG_ERR_WORKSPACE_TOO_SMALL, /**< Caller workspace exhausted; geometry NOT truncated. */
    PG_ERR_DEGENERATE          /**< Valid structure but zero measurable length. */
} pg_result_t;

/** 2D point in path coordinate units (pixels on target). */
typedef struct {
    pg_float_t x; /**< X coordinate (+x right). */
    pg_float_t y; /**< Y coordinate (+y down, LVGL screen convention). */
} pg_point_t;

/** Path command opcodes. */
typedef enum {
    PG_CMD_MOVE = 0, /**< Relocate the cursor (p1 = target). */
    PG_CMD_LINE,     /**< Straight span (p1 = endpoint). */
    PG_CMD_QUAD,     /**< Quadratic Bezier (p1 = control, p2 = endpoint). */
    PG_CMD_CUBIC,    /**< Cubic Bezier (p1/p2 = controls, p3 = endpoint). */
    PG_CMD_CLOSE     /**< Straight span back to the subpath start. */
} pg_cmd_type_t;

/**
 * Single path command (p0 is the cursor position before the command).
 *
 * Only the coordinates consumed by the opcode need to be valid; unused slots
 * may hold any bit pattern. All consumed coordinates must be finite.
 */
typedef struct {
    pg_cmd_type_t type; /**< Opcode, see pg_cmd_type_t. */
    pg_point_t p1;      /**< MOVE/LINE target, QUAD/CUBIC first control. */
    pg_point_t p2;      /**< QUAD endpoint, CUBIC second control. */
    pg_point_t p3;      /**< CUBIC endpoint. */
} pg_cmd_t;

/** A path borrows its command array; the caller keeps it alive. */
typedef struct {
    const pg_cmd_t *cmds; /**< Pointer to path commands (must be kept alive by caller). */
    uint16_t cmd_count;   /**< Total number of commands. */
} pg_path_t;

#define PG_MOVE_TO(x, y) \
    { PG_CMD_MOVE, { (pg_float_t)(x), (pg_float_t)(y) }, { 0.0f, 0.0f }, { 0.0f, 0.0f } }
#define PG_LINE_TO(x, y) \
    { PG_CMD_LINE, { (pg_float_t)(x), (pg_float_t)(y) }, { 0.0f, 0.0f }, { 0.0f, 0.0f } }
#define PG_QUAD_TO(cx, cy, x, y) \
    { PG_CMD_QUAD, { (pg_float_t)(cx), (pg_float_t)(cy) }, \
      { (pg_float_t)(x), (pg_float_t)(y) }, { 0.0f, 0.0f } }
#define PG_CUBIC_TO(c1x, c1y, c2x, c2y, x, y) \
    { PG_CMD_CUBIC, { (pg_float_t)(c1x), (pg_float_t)(c1y) }, \
      { (pg_float_t)(c2x), (pg_float_t)(c2y) }, \
      { (pg_float_t)(x), (pg_float_t)(y) } }
#define PG_CLOSE() \
    { PG_CMD_CLOSE, { 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f } }

#define PG_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/**
 * @brief Translates a result code to a stable human-readable string.
 *
 * @param[in] result  Result code to describe.
 * @return            Pointer to a static string (never NULL).
 */
const char *pg_result_str(pg_result_t result);

#ifdef __cplusplus
}
#endif

#endif /* PATH2D_TYPES_H */
