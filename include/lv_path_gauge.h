/**
 * @file lv_path_gauge.h
 * @brief LVGL 9.1 widget drawing a progress stroke along an arbitrary path.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 *
 * Drawing mirrors LVGL's own lv_line: LV_EVENT_DRAW_MAIN -> lv_event_get_layer
 * -> lv_draw_line_dsc_init -> lv_obj_init_draw_line_dsc -> lv_draw_line.
 * LV_PART_MAIN styles the static track, LV_PART_INDICATOR the active progress.
 * Optional value-domain zones recolour the progress without per-zone objects.
 *
 * The widget never allocates for path geometry: the caller owns the LUT, the
 * flattened polyline and the distance table, and the gauge only borrows them.
 */
#ifndef LV_PATH_GAUGE_H
#define LV_PATH_GAUGE_H

#include <lvgl.h>

#include "path2d/pg_measure.h"
#include "path2d/pg_path.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Recommended LUT capacity for the caller-owned measure storage. */
#ifndef LV_PATH_GAUGE_MAX_SAMPLES
#define LV_PATH_GAUGE_MAX_SAMPLES 128
#endif

/** Recommended render-cache capacity for the caller-owned vertex storage. */
#ifndef LV_PATH_GAUGE_MAX_VERTICES
#define LV_PATH_GAUGE_MAX_VERTICES 128
#endif

/** Minimum render-cache capacity accepted (one segment). */
#define LV_PATH_GAUGE_MIN_VERTICES 2u

/**
 * Fixed zone capacity of one gauge (library build-time constant).
 *
 * Zones live inside the widget instance, never in caller storage, so this
 * capacity does not change any public struct layout; override it only when
 * building the library itself.
 */
#ifndef LV_PATH_GAUGE_MAX_ZONES
#define LV_PATH_GAUGE_MAX_ZONES 8
#endif

/** Tolerance used when the workspace does not select one. */
#define LV_PATH_GAUGE_DEFAULT_TOLERANCE 0.5f

/**
 * Caller-owned storage descriptor for one gauge.
 *
 * The struct stores POINTERS to caller arrays; its layout does not depend on
 * any capacity macro, so the ABI is stable across capacity choices. The
 * pointed-to arrays must stay alive (and unchanged) while the gauge draws.
 *
 * The gauge only reads the descriptor: lv_path_gauge_set_path() never writes
 * to it. Zero-initialize it and fill it with lv_path_gauge_workspace_init().
 */
typedef struct {
    pg_measure_sample_t *samples; /**< Arc-length LUT storage (borrowed). */
    uint16_t samples_capacity;    /**< Elements available in @ref samples. */
    pg_point_t *vertices;         /**< Flattened polyline storage (borrowed). */
    float *distances;             /**< Cumulative distance per vertex (borrowed). */
    uint16_t vertices_capacity;   /**< Elements available in @ref vertices and
                                       @ref distances. */
    float tolerance;              /**< Flatten tolerance in path units; <= 0 selects
                                       LV_PATH_GAUGE_DEFAULT_TOLERANCE. */
} lv_path_gauge_workspace_t;

/**
 * @brief Binds caller storage to a workspace descriptor.
 *
 * @param[out] workspace          Descriptor to fill. Cannot be NULL.
 * @param[in]  samples            Caller LUT storage (borrowed). Cannot be NULL.
 * @param[in]  samples_capacity   Elements in @p samples; below
 *                                PG_MEASURE_MIN_SAMPLES is rejected.
 * @param[in]  vertices           Caller polyline storage (borrowed). Cannot be NULL.
 * @param[in]  distances          Caller distance storage (borrowed). Cannot be NULL.
 * @param[in]  vertices_capacity  Elements in @p vertices / @p distances; below
 *                                LV_PATH_GAUGE_MIN_VERTICES is rejected.
 * @param[in]  tolerance          Flatten tolerance in path units. NaN/Inf is
 *                                rejected; <= 0 selects
 *                                LV_PATH_GAUGE_DEFAULT_TOLERANCE.
 * @return                        PG_OK on success;
 *                                PG_ERR_INVALID_ARG for NULL pointers or a
 *                                non-finite tolerance;
 *                                PG_ERR_WORKSPACE_TOO_SMALL for capacities
 *                                below the minimums.
 *
 * @note Fail-atomic: on any failure @p workspace is zeroed and unusable until
 *       a successful re-init. The arrays themselves are never touched.
 */
pg_result_t lv_path_gauge_workspace_init(lv_path_gauge_workspace_t *workspace,
                                         pg_measure_sample_t *samples,
                                         uint16_t samples_capacity,
                                         pg_point_t *vertices, float *distances,
                                         uint16_t vertices_capacity,
                                         float tolerance);

/**
 * One value-domain colour zone: progress inside [start, end) uses @ref color.
 *
 * Zones are half-open so adjacent zones share a boundary value without
 * overlapping; gaps between zones are allowed and fall back to the
 * LV_PART_INDICATOR base colour.
 */
typedef struct {
    int32_t start;    /**< Zone start value (inclusive). */
    int32_t end;      /**< Zone end value (exclusive); must be greater than
                           @ref start. */
    lv_color_t color; /**< Progress colour inside [start, end). */
} lv_path_gauge_zone_t;

/**
 * @brief Replaces the zone table atomically.
 *
 * Validation happens before any state changes, so a rejected call leaves the
 * previously installed zones untouched:
 *
 * - @p count greater than LV_PATH_GAUGE_MAX_ZONES is rejected.
 * - @p zones must be sorted by ascending start and must not overlap;
 *   adjacent zones (previous end == next start) are allowed, gaps are allowed.
 * - Every zone must satisfy start < end.
 *
 * Zones outside the current value range are NOT rewritten: they are clipped to
 * the range when drawing. Zone colour overrides only the LV_PART_INDICATOR
 * line colour; width, opacity and rounding keep coming from the style.
 *
 * @param[in] obj    Gauge object.
 * @param[in] zones  Zone array to copy; may be NULL only when @p count is 0
 *                   (which clears the zones).
 * @param[in] count  Zones in @p zones; 0 clears the zone table.
 * @return           PG_OK on success;
 *                   PG_ERR_INVALID_ARG for a NULL obj, NULL @p zones with
 *                   count > 0, start >= end, unsorted or overlapping zones;
 *                   PG_ERR_WORKSPACE_TOO_SMALL when @p count exceeds
 *                   LV_PATH_GAUGE_MAX_ZONES.
 */
pg_result_t lv_path_gauge_set_zones(lv_obj_t *obj,
                                    const lv_path_gauge_zone_t *zones,
                                    uint16_t count);

/**
 * @brief Removes all zones (progress returns to the base indicator colour).
 *
 * @param[in] obj  Gauge object.
 * @return         PG_OK on success, PG_ERR_INVALID_ARG when obj is NULL.
 */
pg_result_t lv_path_gauge_clear_zones(lv_obj_t *obj);

/**
 * @brief Creates a path gauge object.
 *
 * The object is a regular lv_obj: set its position/size as usual. Path
 * coordinates are object-local pixels and are never scaled or fitted in v1.
 * Defaults: range 0..100, value 0, no path, no zones.
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
 * outlive the gauge (it is borrowed, never copied).
 *
 * On any failure the gauge is left without a path (no stale geometry) and the
 * returned code explains why:
 *
 * - PG_ERR_INVALID_ARG: NULL obj/path/workspace, NULL storage pointers, or a
 *   non-finite workspace tolerance.
 * - PG_ERR_INVALID_PATH: malformed path, multi-contour or CLOSE present.
 * - PG_ERR_WORKSPACE_TOO_SMALL: capacities below the minimums, or LUT/vertex
 *   capacity exhausted for this path.
 * - PG_ERR_DEGENERATE: no measurable length.
 *
 * Passing path == NULL is a caller error; use lv_path_gauge_clear_path() to
 * clear the gauge explicitly.
 *
 * @param[in] obj        Gauge object.
 * @param[in] path       Single open contour to draw. Cannot be NULL.
 * @param[in] workspace  Caller-owned storage descriptor; borrowed, must
 *                       outlive the gauge. Its tolerance field selects the
 *                       flatness (<= 0 selects the default). The descriptor
 *                       itself is never written to.
 * @return               PG_OK on success, otherwise the error codes above.
 *
 * @note This is the only function that measures and flattens; it is not a
 *       per-frame operation.
 */
pg_result_t lv_path_gauge_set_path(lv_obj_t *obj, const pg_path_t *path,
                                   lv_path_gauge_workspace_t *workspace);

/**
 * @brief Clears the installed path and its runtime metadata.
 *
 * The caller storage stays untouched (the gauge simply stops borrowing it).
 * Safe to call repeatedly and on a gauge that has no path.
 *
 * @param[in] obj  Gauge object.
 * @return         PG_OK on success, PG_ERR_INVALID_ARG when obj is NULL.
 */
pg_result_t lv_path_gauge_clear_path(lv_obj_t *obj);

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

/**
 * @brief Polyline length of the installed path.
 *
 * @param[in] obj  Gauge object.
 * @return         Total cached polyline length in object-local pixels;
 *                 0.0f when obj is NULL or no path is installed.
 */
float lv_path_gauge_get_total_distance(const lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif /* LV_PATH_GAUGE_H */
