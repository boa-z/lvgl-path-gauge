/**
 * @file test_writer.c
 * @brief pg_path_writer_t contract and the fixed-capacity path buffer.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#include "test_util.h"

#include "path2d/pg_path.h"
#include "path2d/pg_writer.h"

#include <math.h>

#define BUF_CAP 8u

static pg_cmd_t g_cmds[BUF_CAP];

static float tu_dist(pg_point_t a, pg_point_t b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;

    return sqrtf(dx * dx + dy * dy);
}

int main(void)
{
    pg_path_buffer_t buffer;
    pg_path_writer_t writer;
    pg_path_t path;
    pg_point_t p = { 1.0f, 2.0f };

    pg_path_buffer_init(&buffer, g_cmds, BUF_CAP);
    TU_EXPECT(buffer.cmds == g_cmds && buffer.capacity == BUF_CAP &&
              buffer.count == 0u && !buffer.overflowed);

    writer = pg_path_buffer_writer(&buffer);
    TU_EXPECT(writer.move_to != NULL && writer.line_to != NULL &&
              writer.quad_to != NULL && writer.cubic_to != NULL &&
              writer.ctx == &buffer);

    /* Paths must start with MOVE. */
    TU_EXPECT(writer.line_to(&buffer, p) == PG_ERR_INVALID_ARG);
    TU_EXPECT(writer.quad_to(&buffer, p, p) == PG_ERR_INVALID_ARG);
    TU_EXPECT(writer.cubic_to(&buffer, p, p, p) == PG_ERR_INVALID_ARG);
    TU_EXPECT(buffer.count == 0u);

    /* Non-finite coordinates are rejected before they reach the buffer. */
    p.x = tu_nan();
    TU_EXPECT(writer.move_to(&buffer, p) == PG_ERR_INVALID_ARG);
    p.x = tu_inf();
    TU_EXPECT(writer.move_to(&buffer, p) == PG_ERR_INVALID_ARG);
    p.x = 1.0f;
    TU_EXPECT(buffer.count == 0u);

    /* Normal round trip: MOVE + LINE + QUAD + CUBIC. */
    TU_EXPECT(writer.move_to(&buffer, (pg_point_t){ 0.0f, 0.0f }) == PG_OK);
    TU_EXPECT(writer.line_to(&buffer, (pg_point_t){ 10.0f, 0.0f }) == PG_OK);
    TU_EXPECT(writer.quad_to(&buffer, (pg_point_t){ 15.0f, 5.0f },
                             (pg_point_t){ 20.0f, 0.0f }) == PG_OK);
    TU_EXPECT(writer.cubic_to(&buffer, (pg_point_t){ 25.0f, 5.0f },
                              (pg_point_t){ 30.0f, 5.0f },
                              (pg_point_t){ 35.0f, 0.0f }) == PG_OK);
    TU_EXPECT(buffer.count == 4u && !buffer.overflowed);
    TU_EXPECT(pg_path_buffer_to_path(&buffer, &path) == PG_OK);
    TU_EXPECT(path.cmd_count == 4u && path.cmds == g_cmds);
    TU_EXPECT(pg_path_validate(&path) == PG_OK);
    TU_EXPECT(path.cmds[0].type == PG_CMD_MOVE);
    TU_EXPECT(path.cmds[1].type == PG_CMD_LINE);
    TU_EXPECT(path.cmds[2].type == PG_CMD_QUAD);
    TU_EXPECT(path.cmds[3].type == PG_CMD_CUBIC);
    TU_POINT_NEAR(path.cmds[2].p1, 15.0f, 5.0f, 1e-6f);
    TU_POINT_NEAR(path.cmds[2].p2, 20.0f, 0.0f, 1e-6f);
    TU_POINT_NEAR(path.cmds[3].p3, 35.0f, 0.0f, 1e-6f);
    TU_EXPECT(tu_dist(path.cmds[1].p1, (pg_point_t){ 10.0f, 0.0f }) < 1e-6f);

    /* Capacity exhaustion: explicit error, nothing silently dropped. */
    pg_path_buffer_init(&buffer, g_cmds, 2u);
    writer = pg_path_buffer_writer(&buffer);
    TU_EXPECT(writer.move_to(&buffer, (pg_point_t){ 0.0f, 0.0f }) == PG_OK);
    TU_EXPECT(writer.line_to(&buffer, (pg_point_t){ 1.0f, 0.0f }) == PG_OK);
    TU_EXPECT(writer.line_to(&buffer, (pg_point_t){ 2.0f, 0.0f }) ==
              PG_ERR_WORKSPACE_TOO_SMALL);
    TU_EXPECT(buffer.count == 2u && buffer.overflowed);
    TU_EXPECT(writer.line_to(&buffer, (pg_point_t){ 3.0f, 0.0f }) ==
              PG_ERR_WORKSPACE_TOO_SMALL);
    TU_EXPECT(buffer.count == 2u);
    /* Incomplete content is never handed out as a path. */
    TU_EXPECT(pg_path_buffer_to_path(&buffer, &path) ==
              PG_ERR_WORKSPACE_TOO_SMALL);

    /* Empty buffer produces no path. */
    pg_path_buffer_init(&buffer, g_cmds, BUF_CAP);
    TU_EXPECT(pg_path_buffer_to_path(&buffer, &path) == PG_ERR_INVALID_PATH);
    TU_EXPECT(pg_path_buffer_to_path(NULL, &path) == PG_ERR_INVALID_ARG);
    TU_EXPECT(pg_path_buffer_to_path(&buffer, NULL) == PG_ERR_INVALID_ARG);

    /* Zero capacity rejects every write. */
    pg_path_buffer_init(&buffer, g_cmds, 0u);
    writer = pg_path_buffer_writer(&buffer);
    TU_EXPECT(writer.move_to(&buffer, (pg_point_t){ 0.0f, 0.0f }) ==
              PG_ERR_WORKSPACE_TOO_SMALL);
    TU_EXPECT(buffer.overflowed && buffer.count == 0u);

    return TU_SUMMARY() ? 1 : 0;
}
