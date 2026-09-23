/**
 * @file pg_writer.c
 * @brief pg_path_writer_t implementations: fixed-capacity command buffer.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "path2d/pg_writer.h"

#include "pg_internal.h"

/* Marks the buffer unusable and remembers the original failure code. The
 * first failure wins; later failures keep reporting it. */
static pg_result_t pg_buffer_poison(pg_path_buffer_t *buffer, pg_result_t code)
{
    if (buffer != NULL && !buffer->poisoned) {
        buffer->poisoned = true;
        buffer->failure = code;
    }
    return code;
}

/* Reserves one slot before writing so capacity failures never leave a
 * partially written command behind and any failure poisons the buffer. */
static pg_result_t pg_buffer_append(pg_path_buffer_t *buffer, const pg_cmd_t *cmd)
{
    if (buffer == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    if (buffer->poisoned) {
        return buffer->failure;
    }
    if (buffer->cmds == NULL || buffer->capacity == 0u) {
        return pg_buffer_poison(buffer, PG_ERR_WORKSPACE_TOO_SMALL);
    }
    if (buffer->count >= buffer->capacity) {
        return pg_buffer_poison(buffer, PG_ERR_WORKSPACE_TOO_SMALL);
    }
    if (cmd->type != PG_CMD_MOVE && buffer->count == 0u) {
        return pg_buffer_poison(buffer, PG_ERR_INVALID_ARG); /* MOVE first */
    }
    if (!pg_cmd_coords_finite(cmd)) {
        return pg_buffer_poison(buffer, PG_ERR_INVALID_ARG); /* never NaN/Inf */
    }
    buffer->cmds[buffer->count] = *cmd;
    buffer->count++;
    return PG_OK;
}

static pg_result_t pg_buffer_move(void *ctx, pg_point_t to)
{
    pg_cmd_t cmd = { PG_CMD_MOVE, to, { 0.0f, 0.0f }, { 0.0f, 0.0f } };

    return pg_buffer_append(ctx, &cmd);
}

static pg_result_t pg_buffer_line(void *ctx, pg_point_t to)
{
    pg_cmd_t cmd = { PG_CMD_LINE, to, { 0.0f, 0.0f }, { 0.0f, 0.0f } };

    return pg_buffer_append(ctx, &cmd);
}

static pg_result_t pg_buffer_quad(void *ctx, pg_point_t control, pg_point_t to)
{
    pg_cmd_t cmd = { PG_CMD_QUAD, control, to, { 0.0f, 0.0f } };

    return pg_buffer_append(ctx, &cmd);
}

static pg_result_t pg_buffer_cubic(void *ctx, pg_point_t control1,
                                   pg_point_t control2, pg_point_t to)
{
    pg_cmd_t cmd = { PG_CMD_CUBIC, control1, control2, to };

    return pg_buffer_append(ctx, &cmd);
}

pg_result_t pg_path_buffer_init(pg_path_buffer_t *buffer, pg_cmd_t *cmds,
                                uint16_t capacity)
{
    if (buffer == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    buffer->cmds = cmds;
    buffer->capacity = capacity;
    buffer->count = 0;
    buffer->poisoned = false;
    buffer->failure = PG_OK;
    if (cmds == NULL || capacity == 0u) {
        /* Safe but unusable: every write reports the reason. */
        return pg_buffer_poison(buffer, PG_ERR_INVALID_ARG);
    }
    return PG_OK;
}

pg_path_writer_t pg_path_buffer_writer(pg_path_buffer_t *buffer)
{
    pg_path_writer_t writer = { NULL, NULL, NULL, NULL, NULL };

    if (buffer == NULL) {
        /* Empty writer: every consumer rejects it with PG_ERR_INVALID_ARG. */
        return writer;
    }
    writer.move_to = pg_buffer_move;
    writer.line_to = pg_buffer_line;
    writer.quad_to = pg_buffer_quad;
    writer.cubic_to = pg_buffer_cubic;
    writer.ctx = buffer;
    return writer;
}

pg_result_t pg_path_buffer_to_path(const pg_path_buffer_t *buffer,
                                   pg_path_t *path)
{
    if (path != NULL) {
        path->cmds = NULL;
        path->cmd_count = 0;
    }
    if (buffer == NULL || path == NULL) {
        return PG_ERR_INVALID_ARG;
    }
    if (buffer->poisoned) {
        /* Return the original failure code; the recorded commands are an
         * incomplete/undefined slice and must not be handed out. */
        return buffer->failure;
    }
    if (buffer->cmds == NULL || buffer->count == 0u) {
        return PG_ERR_INVALID_PATH;
    }
    path->cmds = buffer->cmds;
    path->cmd_count = buffer->count;
    return PG_OK;
}
