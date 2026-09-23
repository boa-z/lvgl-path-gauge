/**
 * @file pg_bezier.h
 * @brief Quadratic/cubic Bezier evaluation, derivatives and subdivision.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#ifndef PATH2D_BEZIER_H
#define PATH2D_BEZIER_H

#include "path2d/pg_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Quadratic Bezier control polygon. */
typedef struct {
    pg_point_t p0; /**< Start point. */
    pg_point_t p1; /**< Control point. */
    pg_point_t p2; /**< End point. */
} pg_quad_t;

/** Cubic Bezier control polygon. */
typedef struct {
    pg_point_t p0; /**< Start point. */
    pg_point_t p1; /**< First control point. */
    pg_point_t p2; /**< Second control point. */
    pg_point_t p3; /**< End point. */
} pg_cubic_t;

/**
 * @brief Evaluates a quadratic Bezier at parameter t (Bernstein basis).
 *
 * @param[in] p0  Start point.
 * @param[in] p1  Control point.
 * @param[in] p2  End point.
 * @param[in] t   Curve parameter; clamped to [0, 1] (NaN clamps to 0).
 * @return        Point B(t) on the curve.
 */
pg_point_t pg_quad_eval(pg_point_t p0, pg_point_t p1, pg_point_t p2, float t);

/**
 * @brief Evaluates the first derivative of a quadratic Bezier at t.
 *
 * @param[in] p0  Start point.
 * @param[in] p1  Control point.
 * @param[in] p2  End point.
 * @param[in] t   Curve parameter; clamped to [0, 1] (NaN clamps to 0).
 * @return        Tangent vector B'(t) (not normalized; may be zero).
 */
pg_point_t pg_quad_derivative(pg_point_t p0, pg_point_t p1, pg_point_t p2,
                              float t);

/**
 * @brief Splits a quadratic Bezier at t using De Casteljau's algorithm.
 *
 * @param[in]  p0     Start point.
 * @param[in]  p1     Control point.
 * @param[in]  p2     End point.
 * @param[in]  t      Split parameter; clamped to [0, 1].
 * @param[out] left   Receives the [0, t] sub-curve. May be NULL (ignored).
 * @param[out] right  Receives the [t, 1] sub-curve. May be NULL (ignored).
 */
void pg_quad_split(pg_point_t p0, pg_point_t p1, pg_point_t p2, float t,
                   pg_quad_t *left, pg_quad_t *right);

/**
 * @brief Evaluates a cubic Bezier at parameter t (Bernstein basis).
 *
 * @param[in] p0  Start point.
 * @param[in] p1  First control point.
 * @param[in] p2  Second control point.
 * @param[in] p3  End point.
 * @param[in] t   Curve parameter; clamped to [0, 1] (NaN clamps to 0).
 * @return        Point B(t) on the curve.
 */
pg_point_t pg_cubic_eval(pg_point_t p0, pg_point_t p1, pg_point_t p2,
                         pg_point_t p3, float t);

/**
 * @brief Evaluates the first derivative of a cubic Bezier at t.
 *
 * @param[in] p0  Start point.
 * @param[in] p1  First control point.
 * @param[in] p2  Second control point.
 * @param[in] p3  End point.
 * @param[in] t   Curve parameter; clamped to [0, 1] (NaN clamps to 0).
 * @return        Tangent vector B'(t) (not normalized; may be zero).
 */
pg_point_t pg_cubic_derivative(pg_point_t p0, pg_point_t p1, pg_point_t p2,
                               pg_point_t p3, float t);

/**
 * @brief Splits a cubic Bezier at t using De Casteljau's algorithm.
 *
 * @param[in]  p0     Start point.
 * @param[in]  p1     First control point.
 * @param[in]  p2     Second control point.
 * @param[in]  p3     End point.
 * @param[in]  t      Split parameter; clamped to [0, 1].
 * @param[out] left   Receives the [0, t] sub-curve. May be NULL (ignored).
 * @param[out] right  Receives the [t, 1] sub-curve. May be NULL (ignored).
 */
void pg_cubic_split(pg_point_t p0, pg_point_t p1, pg_point_t p2, pg_point_t p3,
                    float t, pg_cubic_t *left, pg_cubic_t *right);

#ifdef __cplusplus
}
#endif

#endif /* PATH2D_BEZIER_H */
