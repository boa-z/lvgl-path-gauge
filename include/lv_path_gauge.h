/**
 * @file lv_path_gauge.h
 * @brief LVGL 9.1 widget drawing a progress stroke along an arbitrary path.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#ifndef LV_PATH_GAUGE_H
#define LV_PATH_GAUGE_H

#include <lvgl.h>

#include "path2d/pg_measure.h"
#include "path2d/pg_path.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Arc-length LUT capacity per gauge workspace (override before including). */
#ifndef LV_PATH_GAUGE_MAX_SAMPLES
#define LV_PATH_GAUGE_MAX_SAMPLES 128
#endif

/** Render-cache vertex capacity per gauge workspace (override before including). */
#ifndef LV_PATH_GAUGE_MAX_VERTICES
#define LV_PATH_GAUGE_MAX_VERTICES 128
#endif

/** Tolerance used when the workspace does not select one. */
#define LV_PATH_GAUGE_DEFAULT_TOLERANCE 0.5f

/**
 * Caller-owned storage for one gauge: the arc-length LUT, the flattened
 * render cache and the distance table.
 *
 * Ownership: the workspace is borrowed by the gauge and must stay alive (and
 * unchanged) while the gauge draws. The gauge itself never allocates for path
 * geometry: besides LVGL's own object allocation, all path memory lives here.
 */
typedef struct {
    pg_measure_sample_t samples[LV_PATH_GAUGE_MAX_SAMPLES];
        /**< LUT storage used by measure. */
    pg_measure_t measure;
        /**< Measure view over samples (filled by lv_path_gauge_set_path). */
    pg_point_t vertices[LV_PATH_GAUGE_MAX_VERTICES];
        /**< Flattened polyline in object-local pixels. */
    float distances[LV_PATH_GAUGE_MAX_VERTICES];
        /**< Cumulative arc distance of each vertex. */
    uint16_t vertex_count;
        /**< Vertices in the cache (0 = empty). */
    float total_distance;
        /**< Polyline length used for value -> distance mapping. */
    float tolerance;
        /**< Flatten/measure tolerance in px; <= 0 selects the default. */
} lv_path_gauge_workspace_t;

/**
 * @brief Initializes a workspace (zeroes it and selects a tolerance).
 *
 * @param[out] workspace  Workspace to initialize. Cannot be NULL.
 * @param[in]  tolerance  Flatten tolerance in path units; <= 0 selects
 *                        LV_PATH_GAUGE_DEFAULT_TOLERANCE.
 */
void lv_path_gauge_workspace_init(lv_path_gauge_workspace_t *workspace,
                                  float tolerance);

/**
 * @brief Creates a path gauge object.
 *
 * The object is a regular lv_obj: set its position/size as usual. Path
 * coordinates are object-local pixels and are never scaled or fitted in v1.
 * Defaults: range 0..100, value 0, no path.
 *
 * @param[in] parent  Parent object. Cannot be NULL.
 * @return            Gauge object, or NULL when LVGL could not create it.
 */
lv_obj_t *lv_path_gauge_create(lv_obj_t *parent);

/**
 * @brief Installs a path: builds the measure LUT and the render cache once.
 *
 * Accepted input is a single open contour: exactly one MOVE command, no CLOSE
 * and finite coordinates (pg_path_validate + topology check). The path must
 * outlive the gauge (it is borrowed, never copied). On any failure the gauge
 * is left without a path (no stale geometry) and the returned code explains
 * why:
 *
 * - PG_ERR_INVALID_ARG: NULL obj/workspace.
 * - PG_ERR_INVALID_PATH: malformed path, multi-contour or CLOSE present.
 * - PG_ERR_WORKSPACE_TOO_SMALL: LUT or vertex capacity exhausted.
 * - PG_ERR_DEGENERATE: no measurable length.
 *
 * Passing path == NULL (with any workspace) clears the gauge.
 *
 * @param[in] obj        Gauge object.
 * @param[in] path       Single open contour to draw, or NULL to clear.
 * @param[in] workspace  Caller-owned cache storage; borrowed, must outlive
 *                       the gauge. Its tolerance field selects the flatness.
 * @return               PG_OK on success, otherwise the error codes above.
 *
 * @note This is the only function that measures and flattens; it is not a
 *       per-frame operation.
 */
pg_result_t lv_path_gauge_set_path(lv_obj_t *obj, const pg_path_t *path,
                                   lv_path_gauge_workspace_t *workspace);

/**
 * @brief Sets the value range.
 *
 * @param[in] obj  Gauge object.
 * @param[in] min  Lower bound.
 * @param[in] max  Upper bound; must be greater than min.
 * @return         PG_OK on success, PG_ERR_INVALID_ARG when obj is NULL or
 *                 max <= min (the previous range is kept).
 */
pg_result_t lv_path_gauge_set_range(lv_obj_t *obj, int32_t min, int32_t max);

/**
 * @brief Sets the value, clamped into the current range.
 *
 * Stores the value and invalidates the object only; it never re-measures,
 * re-flattens or touches the geometry cache.
 *
 * @param[in] obj    Gauge object.
 * @param[in] value  Requested value (clamped).
 */
void lv_path_gauge_set_value(lv_obj_t *obj, int32_t value);

/** @brief Current value (clamped range). */
int32_t lv_path_gauge_get_value(const lv_obj_t *obj);

/** @brief Current range minimum. */
int32_t lv_path_gauge_get_min(const lv_obj_t *obj);

/** @brief Current range maximum. */
int32_t lv_path_gauge_get_max(const lv_obj_t *obj);

/**
 * @brief Value mapped into [0, 1] over the current range.
 *
 * Uses int64 arithmetic internally so extreme ranges cannot overflow.
 * Returns 0 when the range is empty.
 */
float lv_path_gauge_get_value_fraction(const lv_obj_t *obj);

/** @brief Vertices in the render cache (0 when no path is set). */
uint16_t lv_path_gauge_get_vertex_count(const lv_obj_t *obj);

/** @brief Polyline length used for value -> distance mapping. */
float lv_path_gauge_get_total_distance(const lv_obj_t *obj);

/** @brief Cached vertex position (object-local pixels). */
pg_point_t lv_path_gauge_get_vertex_point(const lv_obj_t *obj, uint16_t index);

/** @brief Cumulative distance of a cached vertex. */
float lv_path_gauge_get_vertex_distance(const lv_obj_t *obj, uint16_t index);

#ifdef __cplusplus
}
#endif

#endif /* LV_PATH_GAUGE_H */
