/**
 * @file test_bezier.c
 * @brief Bezier evaluate / derivative / split coverage.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "test_util.h"

#include "path2d/pg_bezier.h"

#include <math.h>

static pg_point_t pt(float x, float y)
{
    pg_point_t p = { x, y };

    return p;
}

int main(void)
{
    pg_point_t p;
    pg_quad_t ql;
    pg_quad_t qr;
    pg_cubic_t cl;
    pg_cubic_t cr;

    /* Quad endpoints and midpoint: (0,0),(50,100),(100,0) -> B(0.5)=(50,50). */
    p = pg_quad_eval(pt(0.0f, 0.0f), pt(50.0f, 100.0f), pt(100.0f, 0.0f), 0.0f);
    TU_POINT_NEAR(p, 0.0f, 0.0f, 1e-6f);
    p = pg_quad_eval(pt(0.0f, 0.0f), pt(50.0f, 100.0f), pt(100.0f, 0.0f), 1.0f);
    TU_POINT_NEAR(p, 100.0f, 0.0f, 1e-6f);
    p = pg_quad_eval(pt(0.0f, 0.0f), pt(50.0f, 100.0f), pt(100.0f, 0.0f), 0.5f);
    TU_POINT_NEAR(p, 50.0f, 50.0f, 1e-5f);

    /* Quad derivative at t=0.5 is (100,0), at t=0 it is (100,200). */
    p = pg_quad_derivative(pt(0.0f, 0.0f), pt(50.0f, 100.0f), pt(100.0f, 0.0f),
                           0.5f);
    TU_POINT_NEAR(p, 100.0f, 0.0f, 1e-4f);
    p = pg_quad_derivative(pt(0.0f, 0.0f), pt(50.0f, 100.0f), pt(100.0f, 0.0f),
                           0.0f);
    TU_POINT_NEAR(p, 100.0f, 200.0f, 1e-4f);

    /* Quad split: continuity + endpoint preservation + sub-range mapping. */
    pg_quad_split(pt(0.0f, 0.0f), pt(50.0f, 100.0f), pt(100.0f, 0.0f), 0.5f,
                  &ql, &qr);
    TU_POINT_NEAR(ql.p0, 0.0f, 0.0f, 1e-6f);
    TU_POINT_NEAR(qr.p2, 100.0f, 0.0f, 1e-6f);
    TU_POINT_NEAR(ql.p2, 50.0f, 50.0f, 1e-5f);
    TU_POINT_NEAR(qr.p0, 50.0f, 50.0f, 1e-5f);
    {
        pg_point_t half = pg_quad_eval(ql.p0, ql.p1, ql.p2, 0.25f);
        pg_point_t full = pg_quad_eval(pt(0.0f, 0.0f), pt(50.0f, 100.0f),
                                        pt(100.0f, 0.0f), 0.125f);

        TU_POINT_NEAR(half, full.x, full.y, 1e-4f);
    }

    /* Cubic endpoints and midpoint: B(0.5) = (50,75). */
    p = pg_cubic_eval(pt(0.0f, 0.0f), pt(0.0f, 100.0f), pt(100.0f, 100.0f),
                      pt(100.0f, 0.0f), 0.0f);
    TU_POINT_NEAR(p, 0.0f, 0.0f, 1e-6f);
    p = pg_cubic_eval(pt(0.0f, 0.0f), pt(0.0f, 100.0f), pt(100.0f, 100.0f),
                      pt(100.0f, 0.0f), 1.0f);
    TU_POINT_NEAR(p, 100.0f, 0.0f, 1e-6f);
    p = pg_cubic_eval(pt(0.0f, 0.0f), pt(0.0f, 100.0f), pt(100.0f, 100.0f),
                      pt(100.0f, 0.0f), 0.5f);
    TU_POINT_NEAR(p, 50.0f, 75.0f, 1e-4f);

    /* Cubic derivative at t=0.5 is (150,0). */
    p = pg_cubic_derivative(pt(0.0f, 0.0f), pt(0.0f, 100.0f),
                            pt(100.0f, 100.0f), pt(100.0f, 0.0f), 0.5f);
    TU_POINT_NEAR(p, 150.0f, 0.0f, 1e-3f);

    /* Cubic split: continuity + endpoint preservation. */
    pg_cubic_split(pt(0.0f, 0.0f), pt(0.0f, 100.0f), pt(100.0f, 100.0f),
                   pt(100.0f, 0.0f), 0.5f, &cl, &cr);
    TU_POINT_NEAR(cl.p0, 0.0f, 0.0f, 1e-6f);
    TU_POINT_NEAR(cr.p3, 100.0f, 0.0f, 1e-6f);
    TU_POINT_NEAR(cl.p3, 50.0f, 75.0f, 1e-4f);
    TU_POINT_NEAR(cr.p0, 50.0f, 75.0f, 1e-4f);

    /* Clamp policy: out-of-range and NaN t. */
    p = pg_cubic_eval(pt(0.0f, 0.0f), pt(0.0f, 100.0f), pt(100.0f, 100.0f),
                      pt(100.0f, 0.0f), -2.0f);
    TU_POINT_NEAR(p, 0.0f, 0.0f, 1e-6f);
    p = pg_cubic_eval(pt(0.0f, 0.0f), pt(0.0f, 100.0f), pt(100.0f, 100.0f),
                      pt(100.0f, 0.0f), 5.0f);
    TU_POINT_NEAR(p, 100.0f, 0.0f, 1e-6f);
    p = pg_quad_eval(pt(0.0f, 0.0f), pt(50.0f, 100.0f), pt(100.0f, 0.0f),
                     tu_nan());
    TU_POINT_NEAR(p, 0.0f, 0.0f, 1e-6f);

    /* NULL split outputs are ignored, not crashed on. */
    pg_quad_split(pt(0.0f, 0.0f), pt(1.0f, 1.0f), pt(2.0f, 0.0f), 0.5f, NULL,
                  NULL);
    pg_cubic_split(pt(0.0f, 0.0f), pt(0.0f, 1.0f), pt(1.0f, 1.0f),
                   pt(1.0f, 0.0f), 0.5f, NULL, NULL);
    TU_EXPECT(1);

    return TU_SUMMARY() ? 1 : 0;
}
