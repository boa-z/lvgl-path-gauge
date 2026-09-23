/* SPDX-License-Identifier: MIT */
/* Path structural validation. */
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
    pg_path_t good = { line_cmds, PG_ARRAY_SIZE(line_cmds) };
    pg_path_t empty = { line_cmds, 0 };
    pg_path_t null_cmds = { NULL, 3 };
    pg_path_t bad = { bad_first, PG_ARRAY_SIZE(bad_first) };
    pg_path_t moves = { move_only, PG_ARRAY_SIZE(move_only) };
    pg_path_t closed = { close_path, PG_ARRAY_SIZE(close_path) };
    pg_path_t bad_type;

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

    TU_EXPECT(pg_result_str(PG_OK) != NULL);
    TU_EXPECT(pg_result_str((pg_result_t)12345) != NULL);

    return TU_SUMMARY() ? 1 : 0;
}
