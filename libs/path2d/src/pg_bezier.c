/* SPDX-License-Identifier: MIT */
#include "path2d/pg_bezier.h"

static float pg_clamp01(float t)
{
    if (!(t > 0.0f)) {
        return 0.0f;
    }
    if (!(t < 1.0f)) {
        return 1.0f;
    }
    return t;
}

pg_point_t pg_quad_eval(pg_point_t p0, pg_point_t p1, pg_point_t p2, float t)
{
    float u, a, b, c;
    pg_point_t out;

    t = pg_clamp01(t);
    u = 1.0f - t;
    a = u * u;
    b = 2.0f * u * t;
    c = t * t;
    out.x = a * p0.x + b * p1.x + c * p2.x;
    out.y = a * p0.y + b * p1.y + c * p2.y;
    return out;
}

pg_point_t pg_quad_derivative(pg_point_t p0, pg_point_t p1, pg_point_t p2, float t)
{
    float u;
    pg_point_t out;

    t = pg_clamp01(t);
    u = 1.0f - t;
    out.x = 2.0f * (u * (p1.x - p0.x) + t * (p2.x - p1.x));
    out.y = 2.0f * (u * (p1.y - p0.y) + t * (p2.y - p1.y));
    return out;
}

void pg_quad_split(pg_point_t p0, pg_point_t p1, pg_point_t p2, float t,
                   pg_quad_t *left, pg_quad_t *right)
{
    pg_point_t q0, q1, mid;

    t = pg_clamp01(t);
    q0.x = p0.x + (p1.x - p0.x) * t;
    q0.y = p0.y + (p1.y - p0.y) * t;
    q1.x = p1.x + (p2.x - p1.x) * t;
    q1.y = p1.y + (p2.y - p1.y) * t;
    mid.x = q0.x + (q1.x - q0.x) * t;
    mid.y = q0.y + (q1.y - q0.y) * t;

    if (left != NULL) {
        left->p0 = p0;
        left->p1 = q0;
        left->p2 = mid;
    }
    if (right != NULL) {
        right->p0 = mid;
        right->p1 = q1;
        right->p2 = p2;
    }
}

pg_point_t pg_cubic_eval(pg_point_t p0, pg_point_t p1, pg_point_t p2,
                         pg_point_t p3, float t)
{
    float u, a, b, c, d;
    pg_point_t out;

    t = pg_clamp01(t);
    u = 1.0f - t;
    a = u * u * u;
    b = 3.0f * u * u * t;
    c = 3.0f * u * t * t;
    d = t * t * t;
    out.x = a * p0.x + b * p1.x + c * p2.x + d * p3.x;
    out.y = a * p0.y + b * p1.y + c * p2.y + d * p3.y;
    return out;
}

pg_point_t pg_cubic_derivative(pg_point_t p0, pg_point_t p1, pg_point_t p2,
                               pg_point_t p3, float t)
{
    float u, a, b, c;
    pg_point_t out;

    t = pg_clamp01(t);
    u = 1.0f - t;
    a = u * u;
    b = 2.0f * u * t;
    c = t * t;
    out.x = 3.0f * (a * (p1.x - p0.x) + b * (p2.x - p1.x) + c * (p3.x - p2.x));
    out.y = 3.0f * (a * (p1.y - p0.y) + b * (p2.y - p1.y) + c * (p3.y - p2.y));
    return out;
}

void pg_cubic_split(pg_point_t p0, pg_point_t p1, pg_point_t p2, pg_point_t p3,
                    float t, pg_cubic_t *left, pg_cubic_t *right)
{
    pg_point_t q0, q1, q2, r0, r1, mid;

    t = pg_clamp01(t);
    q0.x = p0.x + (p1.x - p0.x) * t;
    q0.y = p0.y + (p1.y - p0.y) * t;
    q1.x = p1.x + (p2.x - p1.x) * t;
    q1.y = p1.y + (p2.y - p1.y) * t;
    q2.x = p2.x + (p3.x - p2.x) * t;
    q2.y = p2.y + (p3.y - p2.y) * t;
    r0.x = q0.x + (q1.x - q0.x) * t;
    r0.y = q0.y + (q1.y - q0.y) * t;
    r1.x = q1.x + (q2.x - q1.x) * t;
    r1.y = q1.y + (q2.y - q1.y) * t;
    mid.x = r0.x + (r1.x - r0.x) * t;
    mid.y = r0.y + (r1.y - r0.y) * t;

    if (left != NULL) {
        left->p0 = p0;
        left->p1 = q0;
        left->p2 = r0;
        left->p3 = mid;
    }
    if (right != NULL) {
        right->p0 = mid;
        right->p1 = r1;
        right->p2 = q2;
        right->p3 = p3;
    }
}
