/**
 * @file lv_path_gauge_private.h
 * @brief Internal widget instance layout. Applications must not include this.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#ifndef LV_PATH_GAUGE_PRIVATE_H
#define LV_PATH_GAUGE_PRIVATE_H

#include "lv_path_gauge.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Instance structure embedded in lv_obj_t by the class mechanism.
 *
 * All runtime metadata lives here (Phase 4.5 API freeze): the caller only
 * supplies raw storage through lv_path_gauge_workspace_t, whose layout is
 * independent of any capacity macro.
 */
typedef struct {
    lv_obj_t obj;                          /**< Base object (must be first). */
    const pg_path_t *path;                 /**< Borrowed path, NULL when unset. */
    pg_point_t *vertices;                  /**< Borrowed caller render cache. */
    float *distances;                      /**< Borrowed caller distance table. */
    uint16_t vertex_count;                 /**< Vertices written by set_path(). */
    float total_distance;                  /**< Polyline length used for
                                                value -> distance mapping. */
    pg_measure_t measure;                  /**< Arc-length metadata over the
                                                caller LUT (borrowed samples). */
    lv_path_gauge_zone_t zones[LV_PATH_GAUGE_MAX_ZONES]; /**< Active zones. */
    uint16_t zone_count;                   /**< Zones in use (0 = single colour). */
    int32_t min_value;                     /**< Range minimum. */
    int32_t max_value;                     /**< Range maximum. */
    int32_t value;                         /**< Clamped current value. */
} lv_path_gauge_t;

/** Widget class (public for lv_obj type checks). */
extern const lv_obj_class_t lv_path_gauge_class;

#ifdef __cplusplus
}
#endif

#endif /* LV_PATH_GAUGE_PRIVATE_H */
