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
 * Fixed-capacity pg_cmd_t collector. Writes into caller-owned storage and
 * uses no heap.
 *
 * Failure model: the first callback failure of any kind (capacity, missing
 * MOVE, non-finite coordinates, NULL storage) moves the buffer into a
 * poisoned state that remembers the original result code. Poisoned buffers
 * reject every further write with that code and can never be exported.
 */
typedef struct {
    pg_cmd_t *cmds;      /**< Caller-owned command array (borrowed). */
    uint16_t capacity;   /**< Capacity in commands. */
    uint16_t count;      /**< Commands accepted so far. */
    bool poisoned;       /**< Set on the first failure; sticky. */
    pg_result_t failure; /**< Original failure code (PG_OK while healthy). */
} pg_path_buffer_t;

/**
 * @brief Initializes a buffer over caller-owned storage.
 *
 * @param[out] buffer    Buffer to initialize. Cannot be NULL.
 * @param[in]  cmds      Command storage. NULL (or zero capacity) poisons the
 *                       buffer with PG_ERR_INVALID_ARG and makes every write
 *                       fail; no crash.
 * @param[in]  capacity  Capacity in commands.
 * @return               PG_OK when usable; PG_ERR_INVALID_ARG for a NULL
 *                       buffer or unusable storage.
 */
pg_result_t pg_path_buffer_init(pg_path_buffer_t *buffer, pg_cmd_t *cmds,
                                uint16_t capacity);

/**
 * @brief Builds a pg_path_writer_t that appends to `buffer`.
 *
 * @param[in] buffer  Target buffer; must outlive the returned writer.
 * @return            Writer with the four callbacks set and ctx = buffer.
 *                    For a NULL buffer it returns an empty writer (all
 *                    callbacks NULL) so consumers reject it with
 *                    PG_ERR_INVALID_ARG instead of crashing.
 *
 * @note A leading draw command (paths must start with MOVE) and non-finite
 *       coordinates fail with PG_ERR_INVALID_ARG; capacity exhaustion fails
 *       with PG_ERR_WORKSPACE_TOO_SMALL. Every failure poisons the buffer.
 */
pg_path_writer_t pg_path_buffer_writer(pg_path_buffer_t *buffer);

/**
 * @brief Exposes the buffered commands as a pg_path_t view.
 *
 * @param[in]  buffer  Buffer to expose. Cannot be NULL.
 * @param[out] path    Receives a view over buffer->cmds. Cannot be NULL.
 *                     Cleared (NULL/0) on every failure path.
 * @return             PG_OK on success;
 *                     PG_ERR_INVALID_ARG for NULL arguments;
 *                     the original failure code when the buffer was poisoned
 *                     (e.g. PG_ERR_WORKSPACE_TOO_SMALL);
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
