/**
 * @file pg_measure.h
 * @brief Arc-length measurement: LUT build and distance-to-geometry queries.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#ifndef PATH2D_MEASURE_H
#define PATH2D_MEASURE_H

#include "path2d/pg_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Minimum workspace capacity accepted by pg_measure_init(). */
#define PG_MEASURE_MIN_SAMPLES 2u

/**
 * One arc-length table entry.
 *
 * Samples are strictly increasing in distance. Consecutive samples either
 * belong to one command (t interpolates inside it) or form a contour joint
 * (the later sample starts a new command whose local span begins at t = 0).
 */
typedef struct {
    float distance;       /**< Cumulative arc length at this sample, path units. */
    float t;              /**< Curve parameter within command_index, [0, 1]. */
    uint16_t command_index; /**< Index of the owning command in path->cmds. */
} pg_measure_sample_t;

/**
 * Measurement object. Borrows both the path and the workspace: the caller
 * keeps both alive for as long as queries run. Failed init leaves the
 * object unusable (re-init required).
 */
typedef struct {
    const pg_path_t *path;        /**< Measured path (borrowed, Flash-resident). */
    pg_measure_sample_t *samples; /**< Caller-owned LUT workspace (borrowed). */
    uint16_t sample_count;        /**< Samples written by init. */
    uint16_t sample_capacity;     /**< Workspace capacity in samples. */
    float total_length;           /**< Total arc length, path units. */
} pg_measure_t;

/**
 * @brief Builds the arc-length lookup table into caller workspace.
 *
 * Walks the commands (MOVE sets the cursor, CLOSE returns to the subpath
 * start), adaptively subdivides every span and records
 * (distance, t, command_index) samples. Multi-subpath paths accumulate one
 * continuous distance. No heap is used.
 *
 * @param[out] measure          Measure object to initialize.
 * @param[in]  path             Path to measure (validated first).
 * @param[out] workspace        Caller-owned sample array (borrowed, kept).
 * @param[in]  workspace_count  Workspace capacity; below
 *                              PG_MEASURE_MIN_SAMPLES is rejected.
 *                              Guide: 128 samples cover typical instrument
 *                              paths at 0.5-unit tolerance.
 * @param[in]  tolerance        Flatten tolerance in path units (clamped to
 *                              >= PG_MIN_TOLERANCE).
 * @return                      PG_OK on success;
 *                              PG_ERR_INVALID_ARG for NULL pointers or
 *                              non-positive tolerance;
 *                              PG_ERR_INVALID_PATH for malformed paths;
 *                              PG_ERR_WORKSPACE_TOO_SMALL when the table does
 *                              not fit (geometry is never truncated);
 *                              PG_ERR_DEGENERATE for MOVE-only or
 *                              zero-length paths.
 */
pg_result_t pg_measure_init(pg_measure_t *measure, const pg_path_t *path,
                            pg_measure_sample_t *workspace, uint16_t workspace_count,
                            float tolerance);

/**
 * @brief Returns the total arc length of an initialized measure.
 *
 * @param[in] measure  Initialized measure; NULL yields 0.0f.
 * @return             Total length in path units (0.0f if unusable).
 */
float pg_measure_get_length(const pg_measure_t *measure);

/**
 * @brief Returns the exact position and unit tangent at an arc distance.
 *
 * The LUT only locates the bracketing samples (binary search, O(log N));
 * the position comes from evaluating the ORIGINAL curve at the interpolated
 * t, and the tangent from its derivative - so direction stays stable even
 * with few samples. distance is clamped to [0, total_length].
 *
 * @param[in]  measure   Initialized measure. Cannot be NULL.
 * @param[in]  distance  Arc distance in path units (clamped; NaN rejected).
 * @param[out] position  Output point. Cannot be NULL.
 * @param[out] tangent   Output unit tangent (+x right, +y down). Cannot be NULL.
 * @return               PG_OK on success;
 *                       PG_ERR_INVALID_ARG for NULL pointers, NaN distance
 *                       or an uninitialized measure;
 *                       PG_ERR_DEGENERATE if the measure holds no length.
 *
 * @note Zero-derivative queries fall back to the segment chord, then to
 *       (1, 0); results are never NaN/Inf. Cost: one endpoint walk over the
 *       commands plus O(log N) search, one evaluation, one derivative.
 */
pg_result_t pg_measure_get_pos_tan(const pg_measure_t *measure, float distance,
                                   pg_point_t *position, pg_point_t *tangent);

/**
 * @brief Returns the exact position and unit tangent at a normalized distance.
 *
 * @param[in]  measure     Initialized measure. Cannot be NULL.
 * @param[in]  normalized  Normalized distance [0.0, 1.0] (clamped;
 *                         NaN rejected). 0 maps to the path start,
 *                         1 to the path end.
 * @param[out] position    Output point coordinate. Cannot be NULL.
 * @param[out] tangent     Output normalized tangent vector. Cannot be NULL.
 * @return                 PG_OK on success, PG_ERR_INVALID_ARG for NULL
 *                         pointers, NaN input or an uninitialized measure,
 *                         PG_ERR_DEGENERATE if the measure holds no length.
 *
 * @note The tangent vector is normalized (length = 1.0).
 *       In LVGL's coordinate system (+Y is down), the positive normal
 *       (-t.y, t.x) points to the "right" side of the forward direction.
 */
pg_result_t pg_measure_get_pos_tan_normalized(const pg_measure_t *measure,
                                              float normalized,
                                              pg_point_t *position,
                                              pg_point_t *tangent);

/**
 * @brief Normalizes a vector to unit length.
 *
 * @param[in] v  Input vector; the zero vector maps to (1, 0).
 * @return       Unit vector (never NaN/Inf).
 */
pg_point_t pg_vec_normalize(pg_point_t v);

/**
 * @brief Rotates a tangent into its normal (-t.y, t.x).
 *
 * @param[in] tangent  Unit tangent vector.
 * @return             Normal vector (same length as the input).
 */
pg_point_t pg_tangent_to_normal(pg_point_t tangent);

#ifdef __cplusplus
}
#endif

#endif /* PATH2D_MEASURE_H */
