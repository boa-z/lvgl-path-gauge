/**
 * @file pg_writer.c
 * @brief pg_path_writer_t implementations: fixed-capacity command buffer.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "path2d/pg_writer.h"

#include "pg_internal.h"

/* Reserves one slot before writing so capacity failures never leave a
 * partially written command behind. */
static pg_result_t pg_buffer_append(pg_path_buffer_t *buffer, const pg_cmd_t *cmd)
{
    if (buffer->count >= buffer->capacity) {
        buffer->overflowed = true;
        return PG_ERR_WORKSPACE_TOO_SMALL;
    }
    if (cmd->type != PG_CMD_MOVE && buffer->count == 0u) {
        return PG_ERR_INVALID_ARG; /* a path must start with MOVE */
    }
    if (!pg_cmd_coords_finite(cmd)) {
        return PG_ERR_INVALID_ARG; /* never record NaN/Inf geometry */
    }
    buffer->cmds[buffer->count] = *cmd;
    buffer->count++;
    return PG_OK;
}

static pg_result_t pg_buffer_move(void *ctx, pg_point_t to)
{
    pg_path_buffer_t *buffer = ctx;
    pg_cmd_t cmd = { PG_CMD_MOVE, to, { 0.0f, 0.0f }, { 0.0f, 0.0f } };

    return pg_buffer_append(buffer, &cmd);
}

static pg_result_t pg_buffer_line(void *ctx, pg_point_t to)
{
    pg_path_buffer_t *buffer = ctx;
    pg_cmd_t cmd = { PG_CMD_LINE, to, { 0.0f, 0.0f }, { 0.0f, 0.0f } };

    return pg_buffer_append(buffer, &cmd);
}

static pg_result_t pg_buffer_quad(void *ctx, pg_point_t control, pg_point_t to)
{
    pg_path_buffer_t *buffer = ctx;
    pg_cmd_t cmd = { PG_CMD_QUAD, control, to, { 0.0f, 0.0f } };

    return pg_buffer_append(buffer, &cmd);
}

static pg_result_t pg_buffer_cubic(void *ctx, pg_point_t control1,
                                   pg_point_t control2, pg_point_t to)
{
    pg_path_buffer_t *buffer = ctx;
    pg_cmd_t cmd = { PG_CMD_CUBIC, control1, control2, to };

    return pg_buffer_append(buffer, &cmd);
}

void pg_path_buffer_init(pg_path_buffer_t *buffer, pg_cmd_t *cmds,
                         uint16_t capacity)
{
    buffer->cmds = cmds;
    buffer->capacity = capacity;
    buffer->count = 0;
    buffer->overflowed = false;
}

pg_path_writer_t pg_path_buffer_writer(pg_path_buffer_t *buffer)
{
    pg_path_writer_t writer = { pg_buffer_move, pg_buffer_line, pg_buffer_quad,
                                pg_buffer_cubic, buffer };

    return writer;
}

pg_result_t pg_path_buffer_to_path(const pg_path_buffer_t *buffer,
                                   pg_path_t *path)
{
    if (buffer == NULL || path == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    if (buffer->overflowed) {
        /* The recorded commands are an incomplete slice; never hand out a
         * silently truncated path. */
        return PG_ERR_WORKSPACE_TOO_SMALL;
    }
    if (buffer->count == 0u) {
        return PG_ERR_INVALID_PATH;
    }
    path->cmds = buffer->cmds;
    path->cmd_count = buffer->count;
    return PG_OK;
}
