/**
 * @file pg_writer.h
 * @brief Path writer interface and fixed-capacity path buffer.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#ifndef PATH2D_WRITER_H
#define PATH2D_WRITER_H

#include "path2d/pg_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Command sink used by path-producing operations (slices today, SVG import
 * and cache builders later).
 *
 * Every callback returns a result code so sinks can fail with
 * PG_ERR_WORKSPACE_TOO_SMALL and stop the producing operation instead of
 * silently truncating geometry. Callbacks must be called with MOVE first;
 * sinks are expected to reject a leading draw command. A writer never needs
 * a CLOSE callback: slice output is always expressed with explicit points.
 */
typedef struct {
    pg_result_t (*move_to)(void *ctx, pg_point_t to);
        /**< Starts a new subpath at `to`. */
    pg_result_t (*line_to)(void *ctx, pg_point_t to);
        /**< Straight span to `to`. */
    pg_result_t (*quad_to)(void *ctx, pg_point_t control, pg_point_t to);
        /**< Quadratic Bezier span. */
    pg_result_t (*cubic_to)(void *ctx, pg_point_t control1,
                            pg_point_t control2, pg_point_t to);
        /**< Cubic Bezier span. */
    void *ctx; /**< Opaque context forwarded to every callback (borrowed). */
} pg_path_writer_t;

/**
 * Fixed-capacity pg_cmd_t collector. Writes into caller-owned storage, uses
 * no heap and keeps the commands it accepted when a later command overflows
 * (the error is reported through the callback result and the overflowed
 * flag, so the partial content is never silently accepted).
 */
typedef struct {
    pg_cmd_t *cmds;   /**< Caller-owned command array (borrowed). */
    uint16_t capacity; /**< Capacity in commands. */
    uint16_t count;   /**< Commands accepted so far. */
    bool overflowed;  /**< Set once a command was rejected for capacity. */
} pg_path_buffer_t;

/**
 * @brief Initializes a buffer over caller-owned storage.
 *
 * @param[out] buffer    Buffer to initialize. Cannot be NULL.
 * @param[in]  cmds      Command storage. Cannot be NULL for a usable buffer.
 * @param[in]  capacity  Capacity in commands (0 makes every write fail with
 *                       PG_ERR_WORKSPACE_TOO_SMALL).
 */
void pg_path_buffer_init(pg_path_buffer_t *buffer, pg_cmd_t *cmds,
                         uint16_t capacity);

/**
 * @brief Builds a pg_path_writer_t that appends to `buffer`.
 *
 * @param[in] buffer  Target buffer; must outlive the returned writer.
 * @return            Writer with all four callbacks set and ctx = buffer.
 *
 * @note Rejects a leading draw command with PG_ERR_INVALID_ARG (paths must
 *       start with MOVE) and non-finite coordinates with PG_ERR_INVALID_ARG;
 *       on capacity exhaustion returns PG_ERR_WORKSPACE_TOO_SMALL and sets
 *       buffer->overflowed.
 */
pg_path_writer_t pg_path_buffer_writer(pg_path_buffer_t *buffer);

/**
 * @brief Exposes the buffered commands as a pg_path_t view.
 *
 * @param[in]  buffer  Buffer to expose. Cannot be NULL.
 * @param[out] path    Receives a view over buffer->cmds. Cannot be NULL.
 * @return             PG_OK on success;
 *                     PG_ERR_INVALID_ARG for NULL arguments;
 *                     PG_ERR_WORKSPACE_TOO_SMALL if a command was previously
 *                     rejected (the content is incomplete);
 *                     PG_ERR_INVALID_PATH if nothing was written.
 *
 * @note The view borrows buffer storage; re-initializing or reusing the
 *       buffer invalidates it.
 */
pg_result_t pg_path_buffer_to_path(const pg_path_buffer_t *buffer,
                                   pg_path_t *path);

#ifdef __cplusplus
}
#endif

#endif /* PATH2D_WRITER_H */
