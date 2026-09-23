/**
 * @file lv_path_gauge.c
 * @brief LVGL 9.1 arbitrary-path gauge: track, progress and value zones.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 *
 * Drawing follows LVGL 9.1's own lv_line pattern: LV_EVENT_DRAW_MAIN ->
 * lv_event_get_layer() -> lv_draw_line_dsc_init() ->
 * lv_obj_init_draw_line_dsc() -> lv_draw_line(). Only public LVGL API is
 * used. LV_PART_MAIN styles the track, LV_PART_INDICATOR the active progress.
 *
 * Progress is a pure cache slice: the flattened polyline and its cumulative
 * distances are built once by set_path() and only clipped per frame. Optional
 * value-domain zones partition the active run into coloured sub-runs without
 * creating objects or rebuilding geometry. Rounded caps are applied only at
 * the outer start and the true end of the whole active run.
 */
#include "lv_path_gauge_private.h"

#include "path2d/pg_flatten.h"

#include <math.h>

#define MY_CLASS (&lv_path_gauge_class)

/** Render-cache writer context: writes through the gauge's borrowed arrays. */
typedef struct {
    lv_path_gauge_t *gauge; /**< Gauge whose cache is being filled. */
    uint16_t capacity;      /**< Caller vertex capacity. */
    pg_point_t cursor;      /**< Last emitted point. */
    float distance;         /**< Running polyline length. */
} gauge_cache_t;

/** Caps and colour override for one contiguous progress sub-run. */
typedef struct {
    bool round_start;    /**< Apply the rounded style at the sub-run start. */
    bool round_end;      /**< Apply the rounded style at the sub-run end. */
    bool override_color; /**< Use @ref color instead of the part style. */
    lv_color_t color;    /**< Zone colour when @ref override_color is set. */
} gauge_run_style_t;

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

/**
 * Maps a value into [0, 1] with int64 intermediates.
 *
 * The int64 span keeps extreme int32 ranges from overflowing; the only
 * division is a float one, so the per-frame draw path stays free of double
 * arithmetic.
 */
static float gauge_value_fraction(const lv_path_gauge_t *gauge, int32_t value)
{
    int64_t span;
    int64_t offset;

    if (gauge->max_value <= gauge->min_value) {
        return 0.0f;
    }
    span = (int64_t)gauge->max_value - (int64_t)gauge->min_value;
    offset = (int64_t)value - (int64_t)gauge->min_value;
    return (float)offset / (float)span;
}

/** Distance of a value along the cached polyline. */
static float gauge_value_to_distance(const lv_path_gauge_t *gauge, int32_t value)
{
    return gauge_value_fraction(gauge, value) * gauge->total_distance;
}

/* --- render cache builder (pg_path_writer_t sink) -------------------------- */

static pg_result_t gauge_cache_move(void *ctx, pg_point_t to)
{
    gauge_cache_t *cache = ctx;
    lv_path_gauge_t *gauge = cache->gauge;

    if (gauge->vertex_count >= cache->capacity) {
        return PG_ERR_WORKSPACE_TOO_SMALL;
    }
    /* A MOVE only moves the cursor: it starts a contour and adds no length. */
    gauge->vertices[gauge->vertex_count] = to;
    gauge->distances[gauge->vertex_count] = cache->distance;
    gauge->vertex_count++;
    cache->cursor = to;
    return PG_OK;
}

static pg_result_t gauge_cache_line(void *ctx, pg_point_t to)
{
    gauge_cache_t *cache = ctx;
    lv_path_gauge_t *gauge = cache->gauge;

    if (gauge->vertex_count >= cache->capacity) {
        return PG_ERR_WORKSPACE_TOO_SMALL;
    }
    cache->distance += gauge_point_distance(cache->cursor, to);
    gauge->vertices[gauge->vertex_count] = to;
    gauge->distances[gauge->vertex_count] = cache->distance;
    gauge->vertex_count++;
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

pg_result_t lv_path_gauge_workspace_init(lv_path_gauge_workspace_t *workspace,
                                         pg_measure_sample_t *samples,
                                         uint16_t samples_capacity,
                                         pg_point_t *vertices, float *distances,
                                         uint16_t vertices_capacity,
                                         float tolerance)
{
    if (workspace == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    /* Fail-atomic: a rejected init leaves an unusable, zeroed descriptor. */
    *workspace = (lv_path_gauge_workspace_t){ 0 };
    if (samples == NULL || vertices == NULL || distances == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    if (samples_capacity < PG_MEASURE_MIN_SAMPLES ||
        vertices_capacity < LV_PATH_GAUGE_MIN_VERTICES) {
        return PG_ERR_WORKSPACE_TOO_SMALL;
    }
    /* Non-finite tolerance is a caller error everywhere in the API. */
    if (!isfinite(tolerance)) {
        return PG_ERR_INVALID_ARG;
    }
    workspace->samples = samples;
    workspace->samples_capacity = samples_capacity;
    workspace->vertices = vertices;
    workspace->distances = distances;
    workspace->vertices_capacity = vertices_capacity;
    workspace->tolerance =
        (tolerance > 0.0f) ? tolerance : LV_PATH_GAUGE_DEFAULT_TOLERANCE;
    return PG_OK;
}

float lv_path_gauge_get_value_fraction(const lv_obj_t *obj)
{
    const lv_path_gauge_t *gauge = gauge_from_const_obj(obj);

    if (gauge == NULL) {
        return 0.0f;
    }
    return gauge_value_fraction(gauge, gauge->value);
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

/* --- zones ----------------------------------------------------------------- */

pg_result_t lv_path_gauge_set_zones(lv_obj_t *obj,
                                    const lv_path_gauge_zone_t *zones,
                                    uint16_t count)
{
    lv_path_gauge_t *gauge = gauge_from_obj(obj);
    uint16_t i;

    if (gauge == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (count > LV_PATH_GAUGE_MAX_ZONES) {
        return PG_ERR_WORKSPACE_TOO_SMALL;
    }
    if (count > 0u && zones == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    /* Validate everything before touching state: set_zones() is atomic. */
    for (i = 0; i < count; i++) {
        if (zones[i].start >= zones[i].end) {
            return PG_ERR_INVALID_ARG;
        }
        if (i > 0u && zones[i].start < zones[i - 1u].end) {
            /* Unsorted or overlapping; adjacent zones are allowed. */
            return PG_ERR_INVALID_ARG;
        }
    }
    gauge->zone_count = count;
    for (i = 0; i < count; i++) {
        gauge->zones[i] = zones[i];
    }
    lv_obj_invalidate(obj);
    return PG_OK;
}

pg_result_t lv_path_gauge_clear_zones(lv_obj_t *obj)
{
    lv_path_gauge_t *gauge = gauge_from_obj(obj);

    if (gauge == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    LV_ASSERT_OBJ(obj, MY_CLASS);
    gauge->zone_count = 0u;
    lv_obj_invalidate(obj);
    return PG_OK;
}

/* --- path installation ----------------------------------------------------- */

/* Clears path/cache state so a failed set_path() never leaves stale geometry. */
static void gauge_clear_path(lv_obj_t *obj)
{
    lv_path_gauge_t *gauge = gauge_from_obj(obj);

    gauge->path = NULL;
    gauge->vertices = NULL;
    gauge->distances = NULL;
    gauge->vertex_count = 0u;
    gauge->total_distance = 0.0f;
    gauge->measure = (pg_measure_t){ 0 };
    lv_obj_refresh_self_size(obj);
    lv_obj_invalidate(obj);
}

pg_result_t lv_path_gauge_clear_path(lv_obj_t *obj)
{
    lv_path_gauge_t *gauge = gauge_from_obj(obj);

    if (gauge == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    LV_ASSERT_OBJ(obj, MY_CLASS);
    gauge_clear_path(obj);
    return PG_OK;
}

pg_result_t lv_path_gauge_set_path(lv_obj_t *obj, const pg_path_t *path,
                                   lv_path_gauge_workspace_t *workspace)
{
    lv_path_gauge_t *gauge = gauge_from_obj(obj);
    gauge_cache_t cache;
    pg_path_writer_t writer = { gauge_cache_move, gauge_cache_line, NULL, NULL,
                                &cache };
    pg_result_t res;
    float tolerance;
    uint16_t i;
    uint16_t moves = 0;
    uint16_t closes = 0;

    if (gauge == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (path == NULL || workspace == NULL) {
        /* Caller error: fail-atomic like every other failure. */
        gauge_clear_path(obj);
        return PG_ERR_INVALID_ARG;
    }

    if (workspace->samples == NULL || workspace->vertices == NULL ||
        workspace->distances == NULL) {
        gauge_clear_path(obj);
        return PG_ERR_INVALID_ARG;
    }
    if (workspace->samples_capacity < PG_MEASURE_MIN_SAMPLES ||
        workspace->vertices_capacity < LV_PATH_GAUGE_MIN_VERTICES) {
        gauge_clear_path(obj);
        return PG_ERR_WORKSPACE_TOO_SMALL;
    }
    tolerance = workspace->tolerance;
    if (!isfinite(tolerance)) {
        gauge_clear_path(obj);
        return PG_ERR_INVALID_ARG;
    }
    if (!(tolerance > 0.0f)) {
        tolerance = LV_PATH_GAUGE_DEFAULT_TOLERANCE;
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

    res = pg_measure_init(&gauge->measure, path, workspace->samples,
                          workspace->samples_capacity, tolerance);
    if (res != PG_OK) {
        gauge_clear_path(obj);
        return res;
    }

    gauge->vertices = workspace->vertices;
    gauge->distances = workspace->distances;
    gauge->vertex_count = 0u;
    gauge->total_distance = 0.0f;
    cache.gauge = gauge;
    cache.capacity = workspace->vertices_capacity;
    cache.cursor = (pg_point_t){ 0.0f, 0.0f };
    cache.distance = 0.0f;
    res = pg_path_flatten(path, tolerance, &writer);
    if (res != PG_OK) {
        gauge_clear_path(obj);
        return res;
    }
    if (gauge->vertex_count < 2u) {
        gauge_clear_path(obj);
        return PG_ERR_DEGENERATE;
    }
    gauge->total_distance = cache.distance;

    gauge->path = path;
    lv_obj_refresh_self_size(obj);
    lv_obj_invalidate(obj);
    return PG_OK;
}

/* --- drawing --------------------------------------------------------------- */

/**
 * Draws the cached polyline clipped to the distance window [from, to].
 *
 * Sub-ranges are interpolated by their stored cumulative distances, so no
 * measure/slice/flatten work happens per frame. Every geometry joint inside
 * the run is filled with a same-colour round cap (LVGL draws it as a disc of
 * the line width), while the run's own ends stay flat unless the run style
 * asks for the rounded outer cap: semantic boundaries (zone edges, progress
 * end) must never bleed a cap into the neighbouring colour.
 */
static void gauge_draw_run(lv_layer_t *layer, lv_obj_t *obj,
                           const lv_path_gauge_t *gauge, uint32_t part, float from,
                           float to, int32_t x_ofs, int32_t y_ofs,
                           const gauge_run_style_t *style)
{
    lv_draw_line_dsc_t dsc;
    pg_point_t pending_a = { 0.0f, 0.0f };
    pg_point_t pending_b = { 0.0f, 0.0f };
    bool have_pending = false;
    bool flushed_any = false;
    uint16_t i;

    if (gauge->vertex_count < 2u || !(to > from)) {
        return;
    }
    lv_draw_line_dsc_init(&dsc);
    lv_obj_init_draw_line_dsc(obj, part, &dsc);
    if (style->override_color) {
        dsc.color = style->color;
    }

    for (i = 0; i + 1u < gauge->vertex_count; i++) {
        float d0 = gauge->distances[i];
        float d1 = gauge->distances[i + 1u];
        float span = d1 - d0;
        float t0;
        float t1;

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
        if (have_pending) {
            /* Geometry joint: LVGL's round cap is a same-colour disc of the
             * line width centred on the endpoint, so it fills the wedge that
             * a butt joint would leave on the outside of the bend (visible as
             * a background/track seam at thick widths). Semantic boundaries
             * never take this cap: the run ends below keep their flat butt
             * cut. */
            dsc.round_start = 0;
            dsc.round_end = 1;
            dsc.p1.x = gauge_coord(pending_a.x) + x_ofs;
            dsc.p1.y = gauge_coord(pending_a.y) + y_ofs;
            dsc.p2.x = gauge_coord(pending_b.x) + x_ofs;
            dsc.p2.y = gauge_coord(pending_b.y) + y_ofs;
            lv_draw_line(layer, &dsc);
            flushed_any = true;
        }
        pending_a = gauge_lerp(gauge->vertices[i], gauge->vertices[i + 1u], t0);
        pending_b = gauge_lerp(gauge->vertices[i], gauge->vertices[i + 1u], t1);
        have_pending = true;
    }
    if (have_pending) {
        /* Only the outer ends of the whole run carry the rounded style. */
        dsc.round_start = (style->round_start && !flushed_any) ? 1 : 0;
        dsc.round_end = style->round_end ? 1 : 0;
        dsc.p1.x = gauge_coord(pending_a.x) + x_ofs;
        dsc.p1.y = gauge_coord(pending_a.y) + y_ofs;
        dsc.p2.x = gauge_coord(pending_b.x) + x_ofs;
        dsc.p2.y = gauge_coord(pending_b.y) + y_ofs;
        lv_draw_line(layer, &dsc);
    }
}

/**
 * Local overlap compensation for internal colour boundaries.
 *
 * Two butt-capped sub-runs meeting exactly at a zone boundary leave a ~1px
 * antialiased seam in LVGL 9.1's software rasterizer (neither side covers the
 * cut line). Later sub-runs therefore start this many pixels early. The value
 * is a fixed ~1px and is deliberately NOT scaled with the line width.
 */
#define LV_PATH_GAUGE_BOUNDARY_OVERLAP 1.0f

/** Clip start of a sub-run: internal boundaries start one overlap early. */
static float gauge_run_from(float distance)
{
    return (distance > 0.0f) ? (distance - LV_PATH_GAUGE_BOUNDARY_OVERLAP)
                             : 0.0f;
}

/**
 * Partitions [min_value, value] into base/zone sub-runs and draws them.
 *
 * Zones are half-open [start, end) in the value domain and are clipped to the
 * active window at draw time; the stored configuration is never modified.
 * Gaps fall back to the LV_PART_INDICATOR base colour. The rounded indicator
 * style applies only to the outer start and the true end of the whole active
 * run, so internal zone boundaries stay flat; those boundaries get the small
 * fixed overlap above instead of a cap.
 */
static void gauge_draw_zones(lv_layer_t *layer, lv_obj_t *obj,
                             const lv_path_gauge_t *gauge, int32_t x_ofs,
                             int32_t y_ofs, bool rounded)
{
    int32_t v_hi = gauge->value;
    int32_t v_cursor = gauge->min_value;
    bool first = true;
    uint16_t z;

    for (z = 0; z < gauge->zone_count; z++) {
        int32_t zs = LV_MAX(gauge->zones[z].start, gauge->min_value);
        int32_t ze = LV_MIN(gauge->zones[z].end, v_hi);
        gauge_run_style_t style;

        if (ze <= zs) {
            continue; /* no intersection with the active window */
        }
        if (zs > v_cursor) {
            /* Gap: the base indicator colour, never a rounded cap. */
            style = (gauge_run_style_t){ rounded && first, false, false,
                                         lv_color_black() };
            gauge_draw_run(layer, obj, gauge, LV_PART_INDICATOR,
                           gauge_run_from(gauge_value_to_distance(gauge, v_cursor)),
                           gauge_value_to_distance(gauge, zs), x_ofs, y_ofs, &style);
            first = false;
        }
        /* ze >= v_hi means this run carries the true end of the progress. */
        style = (gauge_run_style_t){ rounded && first, rounded && (ze >= v_hi), true,
                                     gauge->zones[z].color };
        gauge_draw_run(layer, obj, gauge, LV_PART_INDICATOR,
                       gauge_run_from(gauge_value_to_distance(gauge, zs)),
                       gauge_value_to_distance(gauge, ze), x_ofs, y_ofs, &style);
        first = false;
        v_cursor = ze;
    }
    if (v_cursor < v_hi) {
        gauge_run_style_t style = { rounded && first, rounded, false,
                                    lv_color_black() };

        gauge_draw_run(layer, obj, gauge, LV_PART_INDICATOR,
                       gauge_run_from(gauge_value_to_distance(gauge, v_cursor)),
                       gauge_value_to_distance(gauge, v_hi), x_ofs, y_ofs, &style);
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

    if (gauge->vertices != NULL) {
        int32_t obj_w = lv_obj_get_width(obj);
        int32_t obj_h = lv_obj_get_height(obj);
        uint16_t i;

        /* A caller may size the object smaller than its path; make sure the
         * stroke is not clipped by the object's invalid area. */
        for (i = 0; i < gauge->vertex_count; i++) {
            int32_t x = (int32_t)gauge_coord(gauge->vertices[i].x);
            int32_t y = (int32_t)gauge_coord(gauge->vertices[i].y);

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
    gauge_run_style_t style;
    bool rounded_main;
    bool rounded_indicator;

    if (gauge->path == NULL || gauge->vertex_count < 2u) {
        return;
    }
    total = gauge->total_distance;
    if (!(total > 0.0f)) {
        return;
    }
    lv_obj_get_coords(obj, &area);
    x_ofs = area.x1 - lv_obj_get_scroll_x(obj);
    y_ofs = area.y1 - lv_obj_get_scroll_y(obj);

    rounded_main = lv_obj_get_style_line_rounded(obj, LV_PART_MAIN) != 0;
    style = (gauge_run_style_t){ rounded_main, rounded_main, false,
                                 lv_color_black() };
    gauge_draw_run(layer, obj, gauge, LV_PART_MAIN, 0.0f, total, x_ofs, y_ofs,
                   &style);

    active = gauge_value_fraction(gauge, gauge->value) * total;
    if (!(active > 0.0f)) {
        return;
    }
    rounded_indicator = lv_obj_get_style_line_rounded(obj, LV_PART_INDICATOR) != 0;
    if (gauge->zone_count == 0u) {
        style = (gauge_run_style_t){ rounded_indicator, rounded_indicator, false,
                                     lv_color_black() };
        gauge_draw_run(layer, obj, gauge, LV_PART_INDICATOR, 0.0f, active, x_ofs,
                       y_ofs, &style);
        return;
    }
    gauge_draw_zones(layer, obj, gauge, x_ofs, y_ofs, rounded_indicator);
}

/* --- class plumbing -------------------------------------------------------- */

static void lv_path_gauge_constructor(const lv_obj_class_t *class_p,
                                      lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    lv_path_gauge_t *gauge = gauge_from_obj(obj);

    gauge->path = NULL;
    gauge->vertices = NULL;
    gauge->distances = NULL;
    gauge->vertex_count = 0u;
    gauge->total_distance = 0.0f;
    gauge->measure = (pg_measure_t){ 0 };
    gauge->zone_count = 0u;
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

        if (gauge->vertices != NULL && gauge->vertex_count > 0u) {
            int32_t w = 0;
            int32_t h = 0;
            uint16_t i;

            for (i = 0; i < gauge->vertex_count; i++) {
                int32_t x = (int32_t)gauge_coord(gauge->vertices[i].x);
                int32_t y = (int32_t)gauge_coord(gauge->vertices[i].y);

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
