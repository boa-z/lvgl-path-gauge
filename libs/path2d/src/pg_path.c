/* SPDX-License-Identifier: MIT */
#include "path2d/pg_path.h"

const char *pg_result_str(pg_result_t result)
{
    switch (result) {
    case PG_OK:
        return "PG_OK";
    case PG_ERR_INVALID_ARG:
        return "PG_ERR_INVALID_ARG";
    case PG_ERR_INVALID_PATH:
        return "PG_ERR_INVALID_PATH";
    case PG_ERR_WORKSPACE_TOO_SMALL:
        return "PG_ERR_WORKSPACE_TOO_SMALL";
    case PG_ERR_DEGENERATE:
        return "PG_ERR_DEGENERATE";
    default:
        return "PG_ERR_UNKNOWN";
    }
}

pg_result_t pg_path_validate(const pg_path_t *path)
{
    uint16_t i;

    if (path == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    if (path->cmd_count == 0 || path->cmds == NULL) {
        return PG_ERR_INVALID_PATH;
    }
    if (path->cmds[0].type != PG_CMD_MOVE) {
        return PG_ERR_INVALID_PATH;
    }
    for (i = 0; i < path->cmd_count; i++) {
        switch (path->cmds[i].type) {
        case PG_CMD_MOVE:
        case PG_CMD_LINE:
        case PG_CMD_QUAD:
        case PG_CMD_CUBIC:
        case PG_CMD_CLOSE:
            break;
        default:
            return PG_ERR_INVALID_PATH;
        }
    }
    return PG_OK;
}
