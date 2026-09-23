/**
 * @file test_path.c
 * @brief Path structural validation, including NaN/Inf coordinate rejection.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "test_util.h"

#include "path2d/pg_path.h"

int main(void)
{
    static const pg_cmd_t line_cmds[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(10.0f, 0.0f),
    };
    static const pg_cmd_t bad_first[] = {
        PG_LINE_TO(10.0f, 0.0f),
    };
    static const pg_cmd_t move_only[] = {
        PG_MOVE_TO(1.0f, 2.0f),
        PG_MOVE_TO(3.0f, 4.0f),
    };
    static const pg_cmd_t close_path[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(10.0f, 0.0f),
        PG_CLOSE(),
    };
    static pg_cmd_t bad_type_cmds[] = {
        PG_MOVE_TO(0.0f, 0.0f),
        PG_LINE_TO(10.0f, 0.0f),
    };
    pg_cmd_t nan_cmds[2] = { PG_MOVE_TO(0.0f, 0.0f), PG_LINE_TO(10.0f, 0.0f) };
    pg_cmd_t inf_cmds[2] = { PG_MOVE_TO(0.0f, 0.0f), PG_LINE_TO(10.0f, 0.0f) };
    pg_cmd_t unused_nan_cmds[2] = { PG_MOVE_TO(0.0f, 0.0f), PG_CLOSE() };
    pg_path_t good = { line_cmds, PG_ARRAY_SIZE(line_cmds) };
    pg_path_t empty = { line_cmds, 0 };
    pg_path_t null_cmds = { NULL, 3 };
    pg_path_t bad = { bad_first, PG_ARRAY_SIZE(bad_first) };
    pg_path_t moves = { move_only, PG_ARRAY_SIZE(move_only) };
    pg_path_t closed = { close_path, PG_ARRAY_SIZE(close_path) };
    pg_path_t bad_type;
    pg_path_t nan_path;
    pg_path_t inf_path;
    pg_path_t unused_nan;

    TU_EXPECT(pg_path_validate(&good) == PG_OK);
    TU_EXPECT(pg_path_validate(NULL) == PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_path_validate(&empty) == PG_ERR_INVALID_PATH);
    TU_EXPECT(pg_path_validate(&null_cmds) == PG_ERR_INVALID_PATH);
    TU_EXPECT(pg_path_validate(&bad) == PG_ERR_INVALID_PATH);
    TU_EXPECT(pg_path_validate(&moves) == PG_OK); /* structurally valid */
    TU_EXPECT(pg_path_validate(&closed) == PG_OK);

    bad_type_cmds[1].type = (pg_cmd_type_t)99;
    bad_type.cmds = bad_type_cmds;
    bad_type.cmd_count = PG_ARRAY_SIZE(bad_type_cmds);
    TU_EXPECT(pg_path_validate(&bad_type) == PG_ERR_INVALID_PATH);

    /* Consumed coordinates must be finite. */
    nan_cmds[1].p1.x = tu_nan();
    nan_path.cmds = nan_cmds;
    nan_path.cmd_count = 2;
    TU_EXPECT(pg_path_validate(&nan_path) == PG_ERR_INVALID_PATH);

    inf_cmds[0].p1.y = tu_inf();
    inf_path.cmds = inf_cmds;
    inf_path.cmd_count = 2;
    TU_EXPECT(pg_path_validate(&inf_path) == PG_ERR_INVALID_PATH);

    /* Only coordinates consumed by the opcode are checked: a MOVE may carry
     * arbitrary garbage in its unused slots, and CLOSE has no coordinates. */
    unused_nan_cmds[0].p2.x = tu_nan();
    unused_nan_cmds[0].p3.y = tu_inf();
    unused_nan_cmds[1].p1.x = tu_nan();
    unused_nan.cmds = unused_nan_cmds;
    unused_nan.cmd_count = 2;
    TU_EXPECT(pg_path_validate(&unused_nan) == PG_OK);

    /* QUAD/CUBIC control coordinates are consumed and must be finite. */
    {
        pg_cmd_t quad_cmds[2] = { PG_MOVE_TO(0.0f, 0.0f),
                                  PG_QUAD_TO(5.0f, 5.0f, 10.0f, 0.0f) };
        pg_cmd_t cubic_cmds[2] = {
            PG_MOVE_TO(0.0f, 0.0f),
            PG_CUBIC_TO(1.0f, 1.0f, 2.0f, 2.0f, 3.0f, 3.0f)
        };
        pg_path_t quad_path = { quad_cmds, 2 };
        pg_path_t cubic_path = { cubic_cmds, 2 };

        quad_cmds[1].p1.x = tu_nan();
        TU_EXPECT(pg_path_validate(&quad_path) == PG_ERR_INVALID_PATH);
        cubic_cmds[1].p3.y = tu_inf();
        TU_EXPECT(pg_path_validate(&cubic_path) == PG_ERR_INVALID_PATH);
    }

    TU_EXPECT(pg_result_str(PG_OK) != NULL);
    TU_EXPECT(pg_result_str((pg_result_t)12345) != NULL);

    return TU_SUMMARY() ? 1 : 0;
}
