/**
 * @file bench_measure.c
 * @brief Host micro-benchmark: LUT build, queries and slice throughput.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 *
 * @note Uses clock() so the resolution is coarse on Windows; iterate counts
 *       are sized to keep each phase above a few hundred milliseconds. The
 *       numbers are order-of-magnitude host figures, not target guarantees.
 */
#include "path2d/pg_measure.h"
#include "path2d/pg_path.h"
#include "path2d/pg_writer.h"

#include <stdio.h>
#include <time.h>

#define WS_CAP 256u
#define QUERIES 2000000L
#define SLICE_ITERS 200000L
#define SLICE_CAP 64u

static pg_measure_sample_t g_ws[WS_CAP];
static pg_cmd_t g_slice_cmds[SLICE_CAP];

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
    pg_path_buffer_t buffer;
    pg_path_writer_t writer;
    double sum = 0.0;
    clock_t t0;
    double measure_secs;
    double query_secs;
    double slice_secs;
    double qps;
    double sps;
    long i;
    float total;

    t0 = clock();
    if (pg_measure_init(&m, &path, g_ws, WS_CAP, 0.25f) != PG_OK) {
        printf("bench: measure_init failed\n");
        return 1;
    }
    measure_secs = (double)(clock() - t0) / (double)CLOCKS_PER_SEC;
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
    query_secs = (double)(clock() - t0) / (double)CLOCKS_PER_SEC;
    qps = (double)QUERIES / (query_secs > 0.0 ? query_secs : 1e-9);

    t0 = clock();
    for (i = 0; i < SLICE_ITERS; i++) {
        float s = total * (float)(i % 100L) / 200.0f;
        float e = total * (float)((i % 100L) + 50L) / 200.0f;

        pg_path_buffer_init(&buffer, g_slice_cmds, SLICE_CAP);
        writer = pg_path_buffer_writer(&buffer);
        if (pg_measure_slice(&m, s, e, &writer) != PG_OK) {
            printf("bench: slice failed\n");
            return 1;
        }
        sum += (double)buffer.count;
    }
    slice_secs = (double)(clock() - t0) / (double)CLOCKS_PER_SEC;
    sps = (double)SLICE_ITERS / (slice_secs > 0.0 ? slice_secs : 1e-9);

    printf("bench: length=%.3f samples=%u workspace_bytes=%u "
           "measure_init_secs=%.6f\n",
           (double)total, m.sample_count,
           (unsigned)(WS_CAP * sizeof(pg_measure_sample_t)), measure_secs);
    printf("bench: queries=%ld queries/sec=%.0f checksum=%.3f\n", QUERIES,
           qps, sum);
    printf("bench: slices=%ld slices/sec=%.0f\n", SLICE_ITERS, sps);
    return 0;
}
