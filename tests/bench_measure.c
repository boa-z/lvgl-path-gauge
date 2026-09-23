/* SPDX-License-Identifier: MIT */
/* Micro-benchmark: measure init cost and query throughput (host only). */
#include "path2d/pg_measure.h"

#include <stdio.h>
#include <time.h>

#define WS_CAP 256u
#define QUERIES 200000L

static pg_measure_sample_t g_ws[WS_CAP];

int main(void)
{
    static const pg_cmd_t s_curve[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_CUBIC_TO(0.0f, 100.0f, 100.0f, 100.0f, 100.0f, 200.0f),
        PG_CUBIC_TO(100.0f, 300.0f, 200.0f, 300.0f, 200.0f, 400.0f),
    };
    pg_path_t path = { s_curve, PG_ARRAY_SIZE(s_curve) };
    pg_measure_t m;
    pg_point_t pos;
    pg_point_t tan;
    double sum = 0.0;
    clock_t t0;
    double secs;
    double qps;
    long i;
    float total;

    if (pg_measure_init(&m, &path, g_ws, WS_CAP, 0.25f) != PG_OK) {
        printf("bench: measure_init failed\n");
        return 1;
    }
    total = pg_measure_get_length(&m);

    t0 = clock();
    for (i = 0; i < QUERIES; i++) {
        float d = total * (float)(i % 1000L) / 1000.0f;

        if (pg_measure_get_pos_tan(&m, d, &pos, &tan) != PG_OK) {
            printf("bench: query failed\n");
            return 1;
        }
        sum += (double)pos.x + (double)pos.y + (double)tan.x;
    }
    secs = (double)(clock() - t0) / (double)CLOCKS_PER_SEC;
    qps = (double)QUERIES / (secs > 0.0 ? secs : 1e-9);

    printf("bench: length=%.3f samples=%u queries=%ld secs=%.3f "
           "queries/sec=%.0f checksum=%.3f\n",
           (double)total, m.sample_count, QUERIES, secs, qps, sum);
    return 0;
}
