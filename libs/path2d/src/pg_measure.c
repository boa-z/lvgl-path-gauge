/**
 * @file pg_measure.c
 * @brief Arc-length measurement: LUT construction and distance queries.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "path2d/pg_measure.h"

#include <math.h>
#include <string.h>

#include "pg_internal.h"

typedef struct {
    pg_measure_t *measure; /**< Target object being built. */
    float dist;            /**< Running arc length. */
} pg_build_t;

static pg_result_t pg_push(pg_build_t *build, float dist, float t,
                           uint16_t command_index)
{
    pg_measure_sample_t *sample;

    if (build->measure->sample_count >= build->measure->sample_capacity) {
        return PG_ERR_WORKSPACE_TOO_SMALL;
    }
    sample = &build->measure->samples[build->measure->sample_count];
    sample->distance = dist;
    sample->t = t;
    sample->command_index = command_index;
    build->measure->sample_count++;
    return PG_OK;
}

/* Accumulates flat-leaf chords. The zero sample is pushed lazily at the
 * first measurable leaf so zero-length prefix commands never become the
 * LUT anchor (and therefore never define the start tangent). */
static pg_result_t pg_measure_leaf(void *ctx, const pg_span_t *span,
                                   pg_span_kind_t kind, uint16_t command_index)
{
    pg_build_t *build = ctx;
    float chord = pg_point_dist(span->p0, span->p3);
    pg_result_t res;

    (void)kind;
    if (chord <= PG_EPSILON) {
        return PG_OK; /* truly degenerate leaf: consumes no distance */
    }
    if (build->measure->sample_count == 0u) {
        res = pg_push(build, 0.0f, span->t0, command_index);
        if (res != PG_OK) {
            return res;
        }
    }
    build->dist += chord;
    return pg_push(build, build->dist, span->t1, command_index);
}

pg_result_t pg_measure_init(pg_measure_t *measure, const pg_path_t *path,
                            pg_measure_sample_t *workspace,
                            uint16_t workspace_count, float tolerance)
{
    pg_build_t build;
    pg_result_t res;

    if (measure == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    /* Fail-atomic: any failure below leaves the object zeroed (unusable)
     * instead of a partially built LUT. */
    memset(measure, 0, sizeof(*measure));

    if (path == NULL || workspace == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    if (workspace_count < PG_MEASURE_MIN_SAMPLES) {
        return PG_ERR_WORKSPACE_TOO_SMALL;
    }
    if (!isfinite(tolerance) || !(tolerance > 0.0f)) {
        return PG_ERR_INVALID_ARG;
    }

    measure->path = path;
    measure->samples = workspace;
    measure->sample_capacity = workspace_count;
    build.measure = measure;
    build.dist = 0.0f;

    res = pg_path_walk(path, tolerance, NULL, pg_measure_leaf, &build);
    if (res != PG_OK) {
        memset(measure, 0, sizeof(*measure));
        return res;
    }
    if (measure->sample_count < PG_MEASURE_MIN_SAMPLES ||
        !(build.dist > PG_EPSILON)) {
        /* MOVE-only path, or every measurably long leaf was degenerate. */
        memset(measure, 0, sizeof(*measure));
        return PG_ERR_DEGENERATE;
    }
    measure->total_length = build.dist;
    return PG_OK;
}

float pg_measure_get_length(const pg_measure_t *measure)
{
    if (measure == NULL) {
        return 0.0f;
    }
    return measure->total_length;
}

pg_point_t pg_vec_normalize(pg_point_t v)
{
    float norm = sqrtf(v.x * v.x + v.y * v.y);
    pg_point_t out;

    if (!(norm > PG_EPSILON)) {
        out.x = 1.0f;
        out.y = 0.0f;
        return out;
    }
    out.x = v.x / norm;
    out.y = v.y / norm;
    return out;
}

pg_point_t pg_tangent_to_normal(pg_point_t tangent)
{
    pg_point_t out;

    out.x = -tangent.y;
    out.y = tangent.x;
    return out;
}

void pg_measure_locate(const pg_measure_t *measure, float distance,
                       pg_locate_t *out)
{
    const pg_measure_sample_t *samples = measure->samples;
    uint16_t lo;
    uint16_t hi;
    uint16_t mid;
    float span;
    float f;

    out->lo = 0;
    out->hi = 0;
    if (measure->sample_count < PG_MEASURE_MIN_SAMPLES) {
        out->command_index = samples[0].command_index;
        out->t = samples[0].t;
        return;
    }
    if (distance <= 0.0f) {
        out->command_index = samples[0].command_index;
        out->t = samples[0].t;
        out->lo = 0;
        out->hi = 1;
        return;
    }
    if (distance >= measure->total_length) {
        uint16_t last = (uint16_t)(measure->sample_count - 1u);

        out->command_index = samples[last].command_index;
        out->t = samples[last].t;
        out->lo = (uint16_t)(last - 1u);
        out->hi = last;
        return;
    }

    lo = 0;
    hi = (uint16_t)(measure->sample_count - 1u);
    while ((uint16_t)(hi - lo) > 1u) {
        mid = (uint16_t)(lo + (hi - lo) / 2u);
        if (samples[mid].distance <= distance) {
            lo = mid;
        }
        else {
            hi = mid;
        }
    }
    span = samples[hi].distance - samples[lo].distance;
    f = (!(span > 0.0f)) ? 1.0f
                         : (distance - samples[lo].distance) / span;
    if (samples[lo].command_index == samples[hi].command_index) {
        out->t = samples[lo].t + (samples[hi].t - samples[lo].t) * f;
    }
    else {
        /* Contour joint: the later command's local span begins at t = 0, so
         * the gap maps linearly onto [0, samples[hi].t]. */
        out->t = samples[hi].t * f;
    }
    out->command_index = samples[hi].command_index;
    out->lo = lo;
    out->hi = hi;
}

pg_point_t pg_sample_pos(const pg_measure_t *measure, uint16_t index)
{
    const pg_measure_sample_t *sample = &measure->samples[index];
    pg_point_t p0;
    pg_point_t end;
    pg_cmd_t cmd;

    pg_cmd_span(measure->path, sample->command_index, &p0, &end, &cmd);
    return pg_cmd_eval(&cmd, p0, end, sample->t);
}

/**
 * Local travel direction extracted from the LUT bracket around a query.
 *
 * This is the second tier of the tangent fallback: when the analytic
 * derivative vanishes (collapsed control handle, cusp), the chord of the
 * whole command can point in a visibly wrong direction (for
 * M(0,0) C(0,0, 0,100, 100,100) it yields the (1,1) diagonal although the
 * curve leaves along +y). The flat leaf the query sits in carries the true
 * local direction, and its endpoints are exactly the bracketing samples.
 */
static bool pg_lut_direction(const pg_measure_t *measure, uint16_t lo,
                             uint16_t hi, pg_point_t *direction)
{
    pg_point_t a;
    pg_point_t b;

    if (measure->sample_count < PG_MEASURE_MIN_SAMPLES) {
        return false;
    }
    if (lo == hi) {
        if ((uint16_t)(hi + 1u) < measure->sample_count) {
            a = pg_sample_pos(measure, hi);
            b = pg_sample_pos(measure, (uint16_t)(hi + 1u));
        }
        else if (lo > 0u) {
            a = pg_sample_pos(measure, (uint16_t)(lo - 1u));
            b = pg_sample_pos(measure, lo);
        }
        else {
            return false;
        }
    }
    else {
        a = pg_sample_pos(measure, lo);
        b = pg_sample_pos(measure, hi);
    }
    if (pg_point_dist(a, b) <= PG_EPSILON) {
        return false;
    }
    direction->x = b.x - a.x;
    direction->y = b.y - a.y;
    *direction = pg_vec_normalize(*direction);
    return true;
}

/* Curve-evaluated position and unit tangent for a located (cmd, t) pair. */
static void pg_located_pos_tan(const pg_measure_t *measure,
                               const pg_locate_t *loc, pg_point_t *position,
                               pg_point_t *tangent)
{
    pg_point_t p0;
    pg_point_t end;
    pg_cmd_t cmd;
    pg_point_t deriv;
    float norm;

    pg_cmd_span(measure->path, loc->command_index, &p0, &end, &cmd);
    *position = pg_cmd_eval(&cmd, p0, end, loc->t);
    if (tangent == NULL) {
        return;
    }
    deriv = pg_cmd_deriv(&cmd, p0, end, loc->t);
    norm = sqrtf(deriv.x * deriv.x + deriv.y * deriv.y);
    if (norm > PG_EPSILON) {
        tangent->x = deriv.x / norm;
        tangent->y = deriv.y / norm;
        return;
    }
    /* Tier 2: direction of the adjacent measurable LUT span. */
    if (pg_lut_direction(measure, loc->lo, loc->hi, tangent)) {
        return;
    }
    /* Tier 3: whole-command chord. */
    deriv.x = end.x - p0.x;
    deriv.y = end.y - p0.y;
    if (pg_point_dist(p0, end) > PG_EPSILON) {
        *tangent = pg_vec_normalize(deriv);
        return;
    }
    /* Tier 4: degenerate everything. */
    tangent->x = 1.0f;
    tangent->y = 0.0f;
}

pg_result_t pg_measure_get_pos_tan(const pg_measure_t *measure, float distance,
                                   pg_point_t *position, pg_point_t *tangent)
{
    pg_locate_t loc;

    if (measure == NULL || position == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    if (measure->samples == NULL || measure->path == NULL ||
        measure->sample_count < PG_MEASURE_MIN_SAMPLES) {
        return PG_ERR_INVALID_ARG;
    }
    if (!(measure->total_length > PG_EPSILON)) {
        return PG_ERR_DEGENERATE;
    }
    if (isnan(distance)) {
        return PG_ERR_INVALID_ARG;
    }
    if (distance < 0.0f) {
        distance = 0.0f;
    }
    if (distance > measure->total_length) {
        distance = measure->total_length;
    }

    pg_measure_locate(measure, distance, &loc);
    pg_located_pos_tan(measure, &loc, position, tangent);
    return PG_OK;
}

pg_result_t pg_measure_get_pos_tan_normalized(const pg_measure_t *measure,
                                              float normalized,
                                              pg_point_t *position,
                                              pg_point_t *tangent)
{
    if (measure == NULL || position == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    if (isnan(normalized)) {
        return PG_ERR_INVALID_ARG;
    }
    if (normalized <= 0.0f) {
        return pg_measure_get_pos_tan(measure, 0.0f, position, tangent);
    }
    if (normalized >= 1.0f) {
        return pg_measure_get_pos_tan(measure, measure->total_length, position,
                                      tangent);
    }
    return pg_measure_get_pos_tan(measure, normalized * measure->total_length,
                                  position, tangent);
}
