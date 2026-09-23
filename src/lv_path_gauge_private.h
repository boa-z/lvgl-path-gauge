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

/** Instance structure embedded in lv_obj_t by the class mechanism. */
typedef struct {
    lv_obj_t obj;                         /**< Base object (must be first). */
    const pg_path_t *path;                /**< Borrowed path, NULL when unset. */
    lv_path_gauge_workspace_t *workspace; /**< Borrowed cache storage. */
    int32_t min_value;                    /**< Range minimum. */
    int32_t max_value;                    /**< Range maximum. */
    int32_t value;                        /**< Clamped current value. */
} lv_path_gauge_t;

/** Widget class (public for lv_obj type checks). */
extern const lv_obj_class_t lv_path_gauge_class;

#ifdef __cplusplus
}
#endif

#endif /* LV_PATH_GAUGE_PRIVATE_H */
