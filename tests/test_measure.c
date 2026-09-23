/* SPDX-License-Identifier: MIT */
/* Path measurement: LUT, binary search, pos/tan, degenerate, workspace. */
#include "test_util.h"
#include "path2d/pg_measure.h"
#include "path2d/pg_path.h"
#include "path2d/pg_bezier.h"

#include <math.h>

#define WS_BIG 256u

static pg_measure_sample_t g_ws[WS_BIG];

static float tu_len(pg_point_t a, pg_point_t b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return sqrtf(dx * dx + dy * dy);
}

/* High-resolution oracle: uniform dense sampling of draw commands. */
static float tu_oracle_length(const pg_cmd_t *cmds, uint16_t n)
{
    pg_point_t cur = { 0.0f, 0.0f };
    pg_point_t start = { 0.0f, 0.0f };
    float total = 0.0f;
    uint16_t i;
    int k;

    for (i = 0; i < n; i++) {
        if (cmds[i].type == PG_CMD_MOVE) {
            cur = cmds[i].p1;
            start = cmds[i].p1;
        }
        else {
            pg_point_t end;
            pg_point_t prev = cur;

            if (cmds[i].type == PG_CMD_LINE) {
                end = cmds[i].p1;
            }
            else if (cmds[i].type == PG_CMD_QUAD) {
                end = cmds[i].p2;
            }
            else if (cmds[i].type == PG_CMD_CUBIC) {
                end = cmds[i].p3;
            }
            else {
                end = start;
            }
            for (k = 1; k <= 20000; k++) {
                float t = (float)k / 20000.0f;
                pg_point_t p;

                if (cmds[i].type == PG_CMD_LINE || cmds[i].type == PG_CMD_CLOSE) {
                    p.x = cur.x + (end.x - cur.x) * t;
                    p.y = cur.y + (end.y - cur.y) * t;
                }
                else if (cmds[i].type == PG_CMD_QUAD) {
                    p = pg_quad_eval(cur, cmds[i].p1, end, t);
                }
                else {
                    p = pg_cubic_eval(cur, cmds[i].p1, cmds[i].p2, end, t);
                }
                total += tu_len(prev, p);
                prev = p;
            }
            cur = end;
        }
    }
    return total;
}

/* Oracle position at distance (same dense sampling, second walk). */
static pg_point_t tu_oracle_pos(const pg_cmd_t *cmds, uint16_t n, float dist)
{
    pg_point_t cur = { 0.0f, 0.0f };
    pg_point_t start = { 0.0f, 0.0f };
    pg_point_t prev = cur;
    float acc = 0.0f;
    uint16_t i;
    int k;

    for (i = 0; i < n; i++) {
        if (cmds[i].type == PG_CMD_MOVE) {
            cur = cmds[i].p1;
            start = cmds[i].p1;
            prev = cur;
        }
        else {
            pg_point_t end;

            if (cmds[i].type == PG_CMD_LINE) {
                end = cmds[i].p1;
            }
            else if (cmds[i].type == PG_CMD_QUAD) {
                end = cmds[i].p2;
            }
            else if (cmds[i].type == PG_CMD_CUBIC) {
                end = cmds[i].p3;
            }
            else {
                end = start;
            }
            for (k = 1; k <= 20000; k++) {
                float t = (float)k / 20000.0f;
                pg_point_t p;

                if (cmds[i].type == PG_CMD_LINE || cmds[i].type == PG_CMD_CLOSE) {
                    p.x = cur.x + (end.x - cur.x) * t;
                    p.y = cur.y + (end.y - cur.y) * t;
                }
                else if (cmds[i].type == PG_CMD_QUAD) {
                    p = pg_quad_eval(cur, cmds[i].p1, end, t);
                }
                else {
                    p = pg_cubic_eval(cur, cmds[i].p1, cmds[i].p2, end, t);
                }
                acc += tu_len(prev, p);
                if (acc >= dist) {
                    return p;
                }
                prev = p;
            }
            cur = end;
        }
    }
    return prev;
}

static void tu_check_fractions(const pg_cmd_t *cmds, uint16_t n,
                               pg_measure_t *m, float total, float pos_tol)
{
    static const float fracs[] = { 0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 1.0f };
    unsigned i;

    for (i = 0; i < sizeof(fracs) / sizeof(fracs[0]); i++) {
        pg_point_t pos;
        pg_point_t tan;
        pg_point_t ref = tu_oracle_pos(cmds, n, fracs[i] * total);
        float tn;

        TU_EXPECT(pg_measure_get_pos_tan_normalized(m, fracs[i], &pos, &tan) ==
                  PG_OK);
        TU_POINT_NEAR(pos, ref.x, ref.y, pos_tol);
        tn = sqrtf(tan.x * tan.x + tan.y * tan.y);
        TU_NEAR(tn, 1.0f, 1e-4f);
        TU_EXPECT(pos.x == pos.x && pos.y == pos.y);
        TU_EXPECT(tan.x == tan.x && tan.y == tan.y);
    }
}

int main(void)
{
    static const pg_cmd_t line_cmds[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(100.0f, 0.0f),
    };
    static const pg_cmd_t vert_cmds[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(0.0f, 100.0f),
    };
    static const pg_cmd_t poly_cmds[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(30.0f, 40.0f),
        PG_LINE_TO(60.0f, 40.0f),
    };
    static const pg_cmd_t square[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(10.0f, 0.0f),
        PG_LINE_TO(10.0f, 10.0f),
        PG_LINE_TO(0.0f, 10.0f),
        PG_CLOSE(),
    };
    static const pg_cmd_t quad_cmds[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_QUAD_TO(50.0f, 100.0f, 100.0f, 0.0f),
    };
    static const pg_cmd_t cubic_cmds[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_CUBIC_TO(0.0f, 100.0f, 100.0f, 100.0f, 100.0f, 0.0f),
    };
    static const pg_cmd_t s_curve[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_CUBIC_TO(0.0f, 100.0f, 100.0f, 100.0f, 100.0f, 200.0f),
        PG_CUBIC_TO(100.0f, 300.0f, 200.0f, 300.0f, 200.0f, 400.0f),
    };
    static const pg_cmd_t two_sub[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(10.0f, 0.0f),
        PG_MOVE_TO(20.0f, 0.0f),
        PG_LINE_TO(30.0f, 0.0f),
    };
    static const pg_cmd_t lead_moves[] = {
        PG_MOVE_TO(5.0f, 5.0f),
        PG_MOVE_TO(10.0f, 10.0f),
        PG_LINE_TO(20.0f, 10.0f),
    };
    static const pg_cmd_t zero_line[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(0.0f, 0.0f),
    };
    static const pg_cmd_t move_only[] = {
        PG_MOVE_TO(1.0f, 2.0f),
    };
    static const pg_cmd_t tiny[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(1e-7f, 0.0f),
    };
    static const pg_cmd_t bad_first[] = {
        PG_LINE_TO(10.0f, 0.0f),
    };
    pg_path_t line = { line_cmds, PG_ARRAY_SIZE(line_cmds) };
    pg_path_t vert = { vert_cmds, PG_ARRAY_SIZE(vert_cmds) };
    pg_path_t poly = { poly_cmds, PG_ARRAY_SIZE(poly_cmds) };
    pg_path_t sq = { square, PG_ARRAY_SIZE(square) };
    pg_path_t quad = { quad_cmds, PG_ARRAY_SIZE(quad_cmds) };
    pg_path_t cubic = { cubic_cmds, PG_ARRAY_SIZE(cubic_cmds) };
    pg_path_t scurve = { s_curve, PG_ARRAY_SIZE(s_curve) };
    pg_path_t two = { two_sub, PG_ARRAY_SIZE(two_sub) };
    pg_path_t leads = { lead_moves, PG_ARRAY_SIZE(lead_moves) };
    pg_path_t zero = { zero_line, PG_ARRAY_SIZE(zero_line) };
    pg_path_t moves = { move_only, PG_ARRAY_SIZE(move_only) };
    pg_path_t tiny_p = { tiny, PG_ARRAY_SIZE(tiny) };
    pg_path_t bad = { bad_first, PG_ARRAY_SIZE(bad_first) };
    pg_path_t empty = { line_cmds, 0 };
    pg_measure_t m;
    pg_measure_sample_t small_ws[4];
    pg_measure_sample_t one_ws[1];
    pg_point_t pos;
    pg_point_t tan;
    float total;
    float ref;

    /* Straight line: exact values. */
    TU_EXPECT(pg_measure_init(&m, &line, g_ws, WS_BIG, 0.5f) == PG_OK);
    TU_EXPECT(m.sample_count >= 2u);
    total = pg_measure_get_length(&m);
    TU_NEAR(total, 100.0f, 1e-4f);
    TU_EXPECT(pg_measure_get_pos_tan(&m, 50.0f, &pos, &tan) == PG_OK);
    TU_POINT_NEAR(pos, 50.0f, 0.0f, 1e-4f);
    TU_POINT_NEAR(tan, 1.0f, 0.0f, 1e-6f);
    TU_EXPECT(pg_measure_get_pos_tan_normalized(&m, 0.0f, &pos, &tan) == PG_OK);
    TU_POINT_NEAR(pos, 0.0f, 0.0f, 1e-6f);
    TU_EXPECT(pg_measure_get_pos_tan_normalized(&m, 1.0f, &pos, &tan) == PG_OK);
    TU_POINT_NEAR(pos, 100.0f, 0.0f, 1e-4f);
    TU_EXPECT(pg_measure_get_pos_tan(&m, -5.0f, &pos, &tan) == PG_OK);
    TU_POINT_NEAR(pos, 0.0f, 0.0f, 1e-6f);
    TU_EXPECT(pg_measure_get_pos_tan(&m, 150.0f, &pos, &tan) == PG_OK);
    TU_POINT_NEAR(pos, 100.0f, 0.0f, 1e-4f);

    /* Vertical line tangent. */
    TU_EXPECT(pg_measure_init(&m, &vert, g_ws, WS_BIG, 0.5f) == PG_OK);
    TU_EXPECT(pg_measure_get_pos_tan(&m, 30.0f, &pos, &tan) == PG_OK);
    TU_POINT_NEAR(pos, 0.0f, 30.0f, 1e-4f);
    TU_POINT_NEAR(tan, 0.0f, 1.0f, 1e-6f);

    /* Polyline accumulation: 50 + 30 = 80. */
    TU_EXPECT(pg_measure_init(&m, &poly, g_ws, WS_BIG, 0.5f) == PG_OK);
    TU_NEAR(pg_measure_get_length(&m), 80.0f, 1e-4f);
    TU_EXPECT(pg_measure_get_pos_tan(&m, 50.0f, &pos, &tan) == PG_OK);
    TU_POINT_NEAR(pos, 30.0f, 40.0f, 1e-3f);
    TU_EXPECT(pg_measure_get_pos_tan(&m, 65.0f, &pos, &tan) == PG_OK);
    TU_POINT_NEAR(pos, 45.0f, 40.0f, 1e-3f);

    /* Closed square: length 40, 75% -> (0,10). */
    TU_EXPECT(pg_measure_init(&m, &sq, g_ws, WS_BIG, 0.5f) == PG_OK);
    TU_NEAR(pg_measure_get_length(&m), 40.0f, 1e-4f);
    TU_EXPECT(pg_measure_get_pos_tan_normalized(&m, 0.75f, &pos, &tan) ==
              PG_OK);
    TU_POINT_NEAR(pos, 0.0f, 10.0f, 1e-3f);

    /* Quad/cubic length vs dense oracle (< 0.2%). */
    TU_EXPECT(pg_measure_init(&m, &quad, g_ws, WS_BIG, 0.25f) == PG_OK);
    total = pg_measure_get_length(&m);
    ref = tu_oracle_length(quad_cmds, PG_ARRAY_SIZE(quad_cmds));
    TU_EXPECT(total > 100.0f);
    TU_NEAR(total / ref, 1.0f, 0.002f);
    tu_check_fractions(quad_cmds, PG_ARRAY_SIZE(quad_cmds), &m, total, 0.5f);

    TU_EXPECT(pg_measure_init(&m, &cubic, g_ws, WS_BIG, 0.25f) == PG_OK);
    total = pg_measure_get_length(&m);
    ref = tu_oracle_length(cubic_cmds, PG_ARRAY_SIZE(cubic_cmds));
    TU_NEAR(total / ref, 1.0f, 0.002f);
    tu_check_fractions(cubic_cmds, PG_ARRAY_SIZE(cubic_cmds), &m, total, 0.5f);

    TU_EXPECT(pg_measure_init(&m, &scurve, g_ws, WS_BIG, 0.25f) == PG_OK);
    total = pg_measure_get_length(&m);
    ref = tu_oracle_length(s_curve, PG_ARRAY_SIZE(s_curve));
    TU_NEAR(total / ref, 1.0f, 0.002f);
    tu_check_fractions(s_curve, PG_ARRAY_SIZE(s_curve), &m, total, 0.6f);
    TU_EXPECT(m.sample_count <= WS_BIG);

    /* Tangent matches numeric derivative direction (dot ~ 1). */
    TU_EXPECT(pg_measure_get_pos_tan_normalized(&m, 0.37f, &pos, &tan) ==
              PG_OK);
    {
        pg_point_t pa;
        pg_point_t pb;
        pg_point_t ta;
        pg_point_t na;
        float e = total * 1e-4f;
        float dot;

        TU_EXPECT(pg_measure_get_pos_tan(&m, 0.37f * total - e, &pa, &ta) ==
                  PG_OK);
        TU_EXPECT(pg_measure_get_pos_tan(&m, 0.37f * total + e, &pb, &ta) ==
                  PG_OK);
        na.x = pb.x - pa.x;
        na.y = pb.y - pa.y;
        na = pg_vec_normalize(na);
        dot = na.x * tan.x + na.y * tan.y;
        TU_EXPECT(dot > 0.999f);
    }

    /* Multi-subpath accumulates one continuous distance. */
    TU_EXPECT(pg_measure_init(&m, &two, g_ws, WS_BIG, 0.5f) == PG_OK);
    TU_NEAR(pg_measure_get_length(&m), 20.0f, 1e-4f);
    TU_EXPECT(pg_measure_get_pos_tan(&m, 15.0f, &pos, &tan) == PG_OK);
    TU_POINT_NEAR(pos, 25.0f, 0.0f, 1e-4f);

    /* Leading MOVEs: start is the last MOVE target. */
    TU_EXPECT(pg_measure_init(&m, &leads, g_ws, WS_BIG, 0.5f) == PG_OK);
    TU_NEAR(pg_measure_get_length(&m), 10.0f, 1e-4f);
    TU_EXPECT(pg_measure_get_pos_tan(&m, 0.0f, &pos, &tan) == PG_OK);
    TU_POINT_NEAR(pos, 10.0f, 10.0f, 1e-6f);

    /* Degenerate inputs: no crash, no NaN/Inf, explicit codes. */
    TU_EXPECT(pg_measure_init(&m, &zero, g_ws, WS_BIG, 0.5f) ==
              PG_ERR_DEGENERATE);
    TU_EXPECT(pg_measure_init(&m, &moves, g_ws, WS_BIG, 0.5f) ==
              PG_ERR_DEGENERATE);
    TU_EXPECT(pg_measure_init(&m, &tiny_p, g_ws, WS_BIG, 0.5f) ==
              PG_ERR_DEGENERATE);
    TU_EXPECT(pg_measure_init(&m, &bad, g_ws, WS_BIG, 0.5f) ==
              PG_ERR_INVALID_PATH);
    TU_EXPECT(pg_measure_init(&m, &empty, g_ws, WS_BIG, 0.5f) ==
              PG_ERR_INVALID_PATH);
    TU_EXPECT(pg_measure_init(NULL, &line, g_ws, WS_BIG, 0.5f) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_measure_init(&m, NULL, g_ws, WS_BIG, 0.5f) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_measure_init(&m, &line, NULL, WS_BIG, 0.5f) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_measure_init(&m, &line, g_ws, 0, 0.5f) ==
              PG_ERR_WORKSPACE_TOO_SMALL);
    TU_EXPECT(pg_measure_init(&m, &line, one_ws, 1, 0.5f) ==
              PG_ERR_WORKSPACE_TOO_SMALL);
    TU_EXPECT(pg_measure_init(&m, &line, g_ws, WS_BIG, 0.0f) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_measure_init(&m, &line, g_ws, WS_BIG, -1.0f) ==
              PG_ERR_INVALID_ARG);

    /* Workspace exhaustion on a real curve: explicit error, no truncation. */
    TU_EXPECT(pg_measure_init(&m, &scurve, small_ws, 4, 0.25f) ==
              PG_ERR_WORKSPACE_TOO_SMALL);

    /* Query-side argument errors. */
    TU_EXPECT(pg_measure_init(&m, &line, g_ws, WS_BIG, 0.5f) == PG_OK);
    TU_EXPECT(pg_measure_get_pos_tan(NULL, 1.0f, &pos, &tan) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_measure_get_pos_tan(&m, 1.0f, NULL, &tan) ==
              PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_measure_get_pos_tan(&m, 1.0f, &pos, NULL) ==
              PG_ERR_INVALID_ARG);
    {
        volatile float zero = 0.0f;
        float nan_v = zero / zero;

        TU_EXPECT(pg_measure_get_pos_tan(&m, nan_v, &pos, &tan) ==
                  PG_ERR_INVALID_ARG);
        TU_EXPECT(pg_measure_get_pos_tan_normalized(&m, nan_v, &pos, &tan) ==
                  PG_ERR_INVALID_ARG);
    }
    TU_EXPECT(pg_measure_get_pos_tan_normalized(&m, -0.5f, &pos, &tan) ==
              PG_OK);
    TU_POINT_NEAR(pos, 0.0f, 0.0f, 1e-6f);
    TU_EXPECT(pg_measure_get_pos_tan_normalized(&m, 1.5f, &pos, &tan) ==
              PG_OK);
    TU_POINT_NEAR(pos, 100.0f, 0.0f, 1e-4f);
    TU_EXPECT(pg_measure_get_length(NULL) == 0.0f);

    /* Vector helpers. */
    tan = pg_vec_normalize((pg_point_t){ 3.0f, 4.0f });
    TU_POINT_NEAR(tan, 0.6f, 0.8f, 1e-6f);
    tan = pg_vec_normalize((pg_point_t){ 0.0f, 0.0f });
    TU_POINT_NEAR(tan, 1.0f, 0.0f, 1e-6f);
    tan = pg_tangent_to_normal((pg_point_t){ 1.0f, 0.0f });
    TU_POINT_NEAR(tan, 0.0f, 1.0f, 1e-6f);

    return TU_SUMMARY() ? 1 : 0;
}
