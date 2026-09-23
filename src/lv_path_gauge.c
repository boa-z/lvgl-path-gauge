/**
 * @file lv_path_gauge.c
 * @brief LVGL 9.1 arbitrary-path gauge: static track + single-colour progress.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 *
 * Drawing follows LVGL 9.1's own lv_line pattern: LV_EVENT_DRAW_MAIN ->
 * lv_event_get_layer() -> lv_draw_line_dsc_init() ->
 * lv_obj_init_draw_line_dsc() -> lv_draw_line(). Only public LVGL API is
 * used. LV_PART_MAIN styles the track, LV_PART_INDICATOR the active progress.
 *
 * v1 scope: one open contour (single MOVE, no CLOSE), no scaling, no zones,
 * no ticks, no needle. Value changes only clamp, store and invalidate.
 */
#include "lv_path_gauge_private.h"

#include "path2d/pg_flatten.h"

#include <math.h>

#define MY_CLASS (&lv_path_gauge_class)

typedef struct {
    lv_path_gauge_workspace_t *workspace; /**< Cache being filled. */
    pg_point_t cursor;                    /**< Last emitted point. */
    float distance;                       /**< Running polyline length. */
} gauge_cache_t;

/* --- small helpers --------------------------------------------------------- */

/** Rounds a float path coordinate into LVGL's precise coordinate type. */
static inline lv_value_precise_t gauge_coord(float value)
{
    return (lv_value_precise_t)(value >= 0.0f ? (value + 0.5f) : (value - 0.5f));
}

static pg_point_t gauge_lerp(pg_point_t a, pg_point_t b, float t)
{
    pg_point_t out;

    out.x = a.x + (b.x - a.x) * t;
    out.y = a.y + (b.y - a.y) * t;
    return out;
}

static float gauge_point_distance(pg_point_t a, pg_point_t b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;

    return sqrtf(dx * dx + dy * dy);
}

/* --- render cache builder (pg_path_writer_t sink) -------------------------- */

static pg_result_t gauge_cache_move(void *ctx, pg_point_t to)
{
    gauge_cache_t *cache = ctx;

    if (cache->workspace->vertex_count >= LV_PATH_GAUGE_MAX_VERTICES) {
        return PG_ERR_WORKSPACE_TOO_SMALL;
    }
    /* A MOVE only moves the cursor: it starts a contour and adds no length. */
    cache->workspace->vertices[cache->workspace->vertex_count] = to;
    cache->workspace->distances[cache->workspace->vertex_count] = cache->distance;
    cache->workspace->vertex_count++;
    cache->cursor = to;
    return PG_OK;
}

static pg_result_t gauge_cache_line(void *ctx, pg_point_t to)
{
    gauge_cache_t *cache = ctx;

    if (cache->workspace->vertex_count >= LV_PATH_GAUGE_MAX_VERTICES) {
        return PG_ERR_WORKSPACE_TOO_SMALL;
    }
    cache->distance += gauge_point_distance(cache->cursor, to);
    cache->workspace->vertices[cache->workspace->vertex_count] = to;
    cache->workspace->distances[cache->workspace->vertex_count] = cache->distance;
    cache->workspace->vertex_count++;
    cache->cursor = to;
    return PG_OK;
}

/* --- value / geometry access ---------------------------------------------- */

static inline lv_path_gauge_t *gauge_from_obj(lv_obj_t *obj)
{
    return (lv_path_gauge_t *)obj;
}

static inline const lv_path_gauge_t *gauge_from_const_obj(const lv_obj_t *obj)
{
    return (const lv_path_gauge_t *)obj;
}

void lv_path_gauge_workspace_init(lv_path_gauge_workspace_t *workspace,
                                  float tolerance)
{
    if (workspace == NULL) {
        return;
    }
    *workspace = (lv_path_gauge_workspace_t){ 0 };
    workspace->tolerance =
        (tolerance > 0.0f) ? tolerance : LV_PATH_GAUGE_DEFAULT_TOLERANCE;
}

float lv_path_gauge_get_value_fraction(const lv_obj_t *obj)
{
    const lv_path_gauge_t *gauge = gauge_from_const_obj(obj);
    int64_t span;
    int64_t offset;

    if (gauge == NULL || gauge->max_value <= gauge->min_value) {
        return 0.0f;
    }
    /* int64 arithmetic keeps extreme int32 ranges from overflowing. */
    span = (int64_t)gauge->max_value - (int64_t)gauge->min_value;
    offset = (int64_t)gauge->value - (int64_t)gauge->min_value;
    return (float)((double)offset / (double)span);
}

uint16_t lv_path_gauge_get_vertex_count(const lv_obj_t *obj)
{
    const lv_path_gauge_t *gauge = gauge_from_const_obj(obj);

    if (gauge == NULL || gauge->workspace == NULL) {
        return 0;
    }
    return gauge->workspace->vertex_count;
}

float lv_path_gauge_get_total_distance(const lv_obj_t *obj)
{
    const lv_path_gauge_t *gauge = gauge_from_const_obj(obj);

    if (gauge == NULL || gauge->workspace == NULL) {
        return 0.0f;
    }
    return gauge->workspace->total_distance;
}

pg_point_t lv_path_gauge_get_vertex_point(const lv_obj_t *obj, uint16_t index)
{
    const lv_path_gauge_t *gauge = gauge_from_const_obj(obj);
    pg_point_t zero = { 0.0f, 0.0f };

    if (gauge == NULL || gauge->workspace == NULL ||
        index >= gauge->workspace->vertex_count) {
        return zero;
    }
    return gauge->workspace->vertices[index];
}

float lv_path_gauge_get_vertex_distance(const lv_obj_t *obj, uint16_t index)
{
    const lv_path_gauge_t *gauge = gauge_from_const_obj(obj);

    if (gauge == NULL || gauge->workspace == NULL ||
        index >= gauge->workspace->vertex_count) {
        return 0.0f;
    }
    return gauge->workspace->distances[index];
}

int32_t lv_path_gauge_get_value(const lv_obj_t *obj)
{
    const lv_path_gauge_t *gauge = gauge_from_const_obj(obj);

    if (gauge == NULL) {
        return 0;
    }
    return gauge->value;
}

int32_t lv_path_gauge_get_min(const lv_obj_t *obj)
{
    const lv_path_gauge_t *gauge = gauge_from_const_obj(obj);

    if (gauge == NULL) {
        return 0;
    }
    return gauge->min_value;
}

int32_t lv_path_gauge_get_max(const lv_obj_t *obj)
{
    const lv_path_gauge_t *gauge = gauge_from_const_obj(obj);

    if (gauge == NULL) {
        return 0;
    }
    return gauge->max_value;
}

void lv_path_gauge_set_value(lv_obj_t *obj, int32_t value)
{
    lv_path_gauge_t *gauge = gauge_from_obj(obj);

    if (gauge == NULL) {
        return;
    }
    LV_ASSERT_OBJ(obj, MY_CLASS);
    /* Clamp only: no measure, no flatten, no cache rebuild. */
    if (value < gauge->min_value) {
        value = gauge->min_value;
    }
    if (value > gauge->max_value) {
        value = gauge->max_value;
    }
    if (value == gauge->value) {
        return;
    }
    gauge->value = value;
    lv_obj_invalidate(obj);
}

pg_result_t lv_path_gauge_set_range(lv_obj_t *obj, int32_t min, int32_t max)
{
    lv_path_gauge_t *gauge = gauge_from_obj(obj);

    if (gauge == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (max <= min) {
        return PG_ERR_INVALID_ARG; /* keep the previous range */
    }
    gauge->min_value = min;
    gauge->max_value = max;
    if (gauge->value < min) {
        gauge->value = min;
    }
    if (gauge->value > max) {
        gauge->value = max;
    }
    lv_obj_invalidate(obj);
    return PG_OK;
}

/* --- path installation ----------------------------------------------------- */

/* Clears path/cache state so a failed set_path() never leaves stale geometry. */
static void gauge_clear_path(lv_obj_t *obj)
{
    lv_path_gauge_t *gauge = gauge_from_obj(obj);

    gauge->path = NULL;
    if (gauge->workspace != NULL) {
        gauge->workspace->vertex_count = 0;
        gauge->workspace->total_distance = 0.0f;
        gauge->workspace->measure = (pg_measure_t){ 0 };
    }
    gauge->workspace = NULL;
    lv_obj_refresh_self_size(obj);
    lv_obj_invalidate(obj);
}

pg_result_t lv_path_gauge_set_path(lv_obj_t *obj, const pg_path_t *path,
                                   lv_path_gauge_workspace_t *workspace)
{
    lv_path_gauge_t *gauge = gauge_from_obj(obj);
    gauge_cache_t cache;
    pg_path_writer_t writer = { gauge_cache_move, gauge_cache_line, NULL, NULL,
                                &cache };
    pg_result_t res;
    uint16_t i;
    uint16_t moves = 0;
    uint16_t closes = 0;

    if (gauge == NULL || workspace == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (path == NULL) {
        gauge_clear_path(obj);
        return PG_OK;
    }

    res = pg_path_validate(path);
    if (res != PG_OK) {
        gauge_clear_path(obj);
        return res;
    }
    for (i = 0; i < path->cmd_count; i++) {
        if (path->cmds[i].type == PG_CMD_MOVE) {
            moves++;
        }
        else if (path->cmds[i].type == PG_CMD_CLOSE) {
            closes++;
        }
    }
    if (moves != 1u || closes != 0u) {
        /* v1 accepts exactly one open contour. */
        gauge_clear_path(obj);
        return PG_ERR_INVALID_PATH;
    }

    if (!(workspace->tolerance > 0.0f)) {
        workspace->tolerance = LV_PATH_GAUGE_DEFAULT_TOLERANCE;
    }
    workspace->vertex_count = 0;
    workspace->total_distance = 0.0f;

    res = pg_measure_init(&workspace->measure, path, workspace->samples,
                          LV_PATH_GAUGE_MAX_SAMPLES, workspace->tolerance);
    if (res != PG_OK) {
        gauge_clear_path(obj);
        return res;
    }

    cache.workspace = workspace;
    cache.cursor = (pg_point_t){ 0.0f, 0.0f };
    cache.distance = 0.0f;
    res = pg_path_flatten(path, workspace->tolerance, &writer);
    if (res != PG_OK) {
        gauge_clear_path(obj);
        return res;
    }
    if (workspace->vertex_count < 2u) {
        gauge_clear_path(obj);
        return PG_ERR_DEGENERATE;
    }
    workspace->total_distance = cache.distance;

    gauge->path = path;
    gauge->workspace = workspace;
    lv_obj_refresh_self_size(obj);
    lv_obj_invalidate(obj);
    return PG_OK;
}

/* --- drawing --------------------------------------------------------------- */

/**
 * Draws the cached polyline clipped to the distance window [from, to].
 *
 * Progress is a pure cache slice: segments are interpolated by their stored
 * cumulative distances, so no measure/slice/flatten work happens per frame.
 */
static void gauge_draw_range(lv_layer_t *layer, lv_obj_t *obj,
                             const lv_path_gauge_workspace_t *workspace,
                             uint32_t part, float from, float to, int32_t x_ofs,
                             int32_t y_ofs)
{
    lv_draw_line_dsc_t dsc;
    bool started = false;
    uint16_t i;

    if (workspace == NULL || workspace->vertex_count < 2u || !(to > from)) {
        return;
    }
    lv_draw_line_dsc_init(&dsc);
    lv_obj_init_draw_line_dsc(obj, part, &dsc);

    for (i = 0; i + 1u < workspace->vertex_count; i++) {
        float d0 = workspace->distances[i];
        float d1 = workspace->distances[i + 1u];
        float span = d1 - d0;
        float t0;
        float t1;
        pg_point_t a;
        pg_point_t b;

        if (!(span > 0.0f) || d1 <= from || d0 >= to) {
            continue;
        }
        t0 = (from - d0) / span;
        if (t0 < 0.0f) {
            t0 = 0.0f;
        }
        t1 = (to - d0) / span;
        if (t1 > 1.0f) {
            t1 = 1.0f;
        }
        if (!(t1 > t0)) {
            continue;
        }
        a = gauge_lerp(workspace->vertices[i], workspace->vertices[i + 1u], t0);
        b = gauge_lerp(workspace->vertices[i], workspace->vertices[i + 1u], t1);

        dsc.p1.x = gauge_coord(a.x) + x_ofs;
        dsc.p1.y = gauge_coord(a.y) + y_ofs;
        dsc.p2.x = gauge_coord(b.x) + x_ofs;
        dsc.p2.y = gauge_coord(b.y) + y_ofs;
        if (started) {
            /* Round caps only on the outer ends of the drawn run, like
             * lv_line does, so joins stay continuous. */
            dsc.round_start = 0;
        }
        lv_draw_line(layer, &dsc);
        started = true;
    }
}

static int32_t gauge_ext_draw_size(const lv_obj_t *obj,
                                   const lv_path_gauge_t *gauge)
{
    int32_t width_main = lv_obj_get_style_line_width(obj, LV_PART_MAIN);
    int32_t width_indicator =
        lv_obj_get_style_line_width(obj, LV_PART_INDICATOR);
    int32_t width = LV_MAX(width_main, width_indicator);
    /* Half a stroke on both sides plus one pixel of antialiasing slack. */
    int32_t extra = width / 2 + 2;

    if (gauge->workspace != NULL) {
        int32_t obj_w = lv_obj_get_width(obj);
        int32_t obj_h = lv_obj_get_height(obj);
        uint16_t i;

        /* A caller may size the object smaller than its path; make sure the
         * stroke is not clipped by the object's invalid area. */
        for (i = 0; i < gauge->workspace->vertex_count; i++) {
            int32_t x = (int32_t)gauge_coord(gauge->workspace->vertices[i].x);
            int32_t y = (int32_t)gauge_coord(gauge->workspace->vertices[i].y);

            if (x < 0) {
                extra = LV_MAX(extra, -x + width);
            }
            else if (x > obj_w) {
                extra = LV_MAX(extra, x - obj_w + width);
            }
            if (y < 0) {
                extra = LV_MAX(extra, -y + width);
            }
            else if (y > obj_h) {
                extra = LV_MAX(extra, y - obj_h + width);
            }
        }
    }
    return extra;
}

static void gauge_draw_event(lv_event_t *e, lv_obj_t *obj,
                             lv_path_gauge_t *gauge)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t area;
    int32_t x_ofs;
    int32_t y_ofs;
    float total;
    float active;

    if (gauge->path == NULL || gauge->workspace == NULL ||
        gauge->workspace->vertex_count < 2u) {
        return;
    }
    total = gauge->workspace->total_distance;
    if (!(total > 0.0f)) {
        return;
    }
    lv_obj_get_coords(obj, &area);
    x_ofs = area.x1 - lv_obj_get_scroll_x(obj);
    y_ofs = area.y1 - lv_obj_get_scroll_y(obj);

    gauge_draw_range(layer, obj, gauge->workspace, LV_PART_MAIN, 0.0f, total,
                     x_ofs, y_ofs);

    active = lv_path_gauge_get_value_fraction(obj) * total;
    if (active > 0.0f) {
        gauge_draw_range(layer, obj, gauge->workspace, LV_PART_INDICATOR, 0.0f,
                         active, x_ofs, y_ofs);
    }
}

/* --- class plumbing -------------------------------------------------------- */

static void lv_path_gauge_constructor(const lv_obj_class_t *class_p,
                                      lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    lv_path_gauge_t *gauge = gauge_from_obj(obj);

    gauge->path = NULL;
    gauge->workspace = NULL;
    gauge->min_value = 0;
    gauge->max_value = 100;
    gauge->value = 0;

    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_set_style_line_width(obj, 4, LV_PART_MAIN);
    lv_obj_set_style_line_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_line_color(obj, lv_color_hex(0x606060), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(obj, true, LV_PART_MAIN);

    lv_obj_set_style_line_width(obj, 4, LV_PART_INDICATOR);
    lv_obj_set_style_line_opa(obj, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_line_color(obj, lv_color_hex(0x0091FF), LV_PART_INDICATOR);
    lv_obj_set_style_line_rounded(obj, true, LV_PART_INDICATOR);
}

static void lv_path_gauge_event(const lv_obj_class_t *class_p, lv_event_t *e)
{
    lv_result_t res;

    LV_UNUSED(class_p);
    /*Call the ancestor's event handler*/
    res = lv_obj_event_base(MY_CLASS, e);
    if (res != LV_RESULT_OK) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_current_target(e);
    lv_path_gauge_t *gauge = gauge_from_obj(obj);

    if (code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        lv_event_set_ext_draw_size(e, gauge_ext_draw_size(obj, gauge));
    }
    else if (code == LV_EVENT_GET_SELF_SIZE) {
        lv_point_t *size = lv_event_get_param(e);

        if (gauge->workspace != NULL && gauge->workspace->vertex_count > 0u) {
            int32_t w = 0;
            int32_t h = 0;
            uint16_t i;

            for (i = 0; i < gauge->workspace->vertex_count; i++) {
                int32_t x =
                    (int32_t)gauge_coord(gauge->workspace->vertices[i].x);
                int32_t y =
                    (int32_t)gauge_coord(gauge->workspace->vertices[i].y);

                if (x > w) {
                    w = x;
                }
                if (y > h) {
                    h = y;
                }
            }
            size->x = w;
            size->y = h;
        }
    }
    else if (code == LV_EVENT_DRAW_MAIN) {
        gauge_draw_event(e, obj, gauge);
    }
}

const lv_obj_class_t lv_path_gauge_class = {
    .constructor_cb = lv_path_gauge_constructor,
    .event_cb = lv_path_gauge_event,
    .width_def = LV_SIZE_CONTENT,
    .height_def = LV_SIZE_CONTENT,
    .instance_size = sizeof(lv_path_gauge_t),
    .base_class = &lv_obj_class,
    .name = "path_gauge",
};

lv_obj_t *lv_path_gauge_create(lv_obj_t *parent)
{
    lv_obj_t *obj;

    if (parent == NULL) {
        return NULL;
    }
    obj = lv_obj_class_create_obj(MY_CLASS, parent);
    if (obj == NULL) {
        return NULL;
    }
    lv_obj_class_init_obj(obj);
    return obj;
}
