/* SPDX-License-Identifier: MIT */
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>

static int tu_checks;
static int tu_failed;

#define TU_EXPECT(cond) \
    do { \
        tu_checks++; \
        if (!(cond)) { \
            tu_failed++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

#define TU_NEAR(a, b, eps) \
    TU_EXPECT(!(((a) - (b) > (eps)) || ((b) - (a) > (eps))))

#define TU_POINT_NEAR(p, ex, ey, eps) \
    do { \
        TU_NEAR((p).x, (ex), (eps)); \
        TU_NEAR((p).y, (ey), (eps)); \
    } while (0)

#define TU_SUMMARY() \
    (printf("%s: %d checks, %d failures\n", __FILE__, tu_checks, tu_failed), \
     tu_failed)

#endif /* TEST_UTIL_H */
