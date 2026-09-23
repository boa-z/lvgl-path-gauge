/**
 * @file pg_path.c
 * @brief Structural validation and command-query helpers for paths.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "path2d/pg_path.h"

#include "pg_internal.h"

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

/* Only coordinates that the opcode actually consumes are validated;
 * unused slots may hold any bit pattern. */
bool pg_cmd_coords_finite(const pg_cmd_t *cmd)
{
    switch (cmd->type) {
    case PG_CMD_MOVE:
    case PG_CMD_LINE:
        return pg_point_is_finite(cmd->p1);
    case PG_CMD_QUAD:
        return pg_point_is_finite(cmd->p1) && pg_point_is_finite(cmd->p2);
    case PG_CMD_CUBIC:
        return pg_point_is_finite(cmd->p1) && pg_point_is_finite(cmd->p2) &&
               pg_point_is_finite(cmd->p3);
    case PG_CMD_CLOSE:
        return true; /* no coordinates */
    default:
        return false;
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
        const pg_cmd_t *cmd = &path->cmds[i];

        switch (cmd->type) {
        case PG_CMD_MOVE:
        case PG_CMD_LINE:
        case PG_CMD_QUAD:
        case PG_CMD_CUBIC:
        case PG_CMD_CLOSE:
            break;
        default:
            return PG_ERR_INVALID_PATH;
        }
        if (!pg_cmd_coords_finite(cmd)) {
            return PG_ERR_INVALID_PATH;
        }
    }
    return PG_OK;
}

void pg_cmd_span(const pg_path_t *path, uint16_t index, pg_point_t *p0,
                 pg_point_t *end, pg_cmd_t *cmd)
{
    pg_point_t cursor = { 0.0f, 0.0f };
    pg_point_t start = { 0.0f, 0.0f };
    uint16_t i;

    *p0 = path->cmds[0].p1;
    *end = path->cmds[0].p1;
    *cmd = path->cmds[0];
    for (i = 0; i < path->cmd_count; i++) {
        const pg_cmd_t *c = &path->cmds[i];
        pg_point_t span_end;

        if (c->type == PG_CMD_MOVE) {
            cursor = c->p1;
            start = c->p1;
            continue;
        }
        switch (c->type) {
        case PG_CMD_LINE:
            span_end = c->p1;
            break;
        case PG_CMD_QUAD:
            span_end = c->p2;
            break;
        case PG_CMD_CUBIC:
            span_end = c->p3;
            break;
        default: /* PG_CMD_CLOSE */
            span_end = start;
            break;
        }
        if (i == index) {
            *p0 = cursor;
            *end = span_end;
            *cmd = *c;
            return;
        }
        cursor = span_end;
    }
}

pg_point_t pg_cmd_eval(const pg_cmd_t *cmd, pg_point_t p0, pg_point_t end,
                       float t)
{
    pg_point_t out;

    switch (cmd->type) {
    case PG_CMD_QUAD:
        return pg_quad_eval(p0, cmd->p1, cmd->p2, t);
    case PG_CMD_CUBIC:
        return pg_cubic_eval(p0, cmd->p1, cmd->p2, cmd->p3, t);
    default: /* LINE and CLOSE are straight spans */
        out.x = p0.x + (end.x - p0.x) * t;
        out.y = p0.y + (end.y - p0.y) * t;
        return out;
    }
}

pg_point_t pg_cmd_deriv(const pg_cmd_t *cmd, pg_point_t p0, pg_point_t end,
                        float t)
{
    pg_point_t out;

    switch (cmd->type) {
    case PG_CMD_QUAD:
        return pg_quad_derivative(p0, cmd->p1, cmd->p2, t);
    case PG_CMD_CUBIC:
        return pg_cubic_derivative(p0, cmd->p1, cmd->p2, cmd->p3, t);
    default:
        out.x = end.x - p0.x;
        out.y = end.y - p0.y;
        return out;
    }
}
