/**
 * @file pg_path.h
 * @brief Structural validation for pg_path_t command streams.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 */
#ifndef PATH2D_PATH_H
#define PATH2D_PATH_H

#include "path2d/pg_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Validates path structure and coordinate finiteness.
 *
 * Rules: path non-NULL, cmd_count > 0, cmds non-NULL, the first command is
 * MOVE, every opcode is known, and every coordinate consumed by its opcode is
 * finite (no NaN/Inf). Unused coordinate slots are ignored.
 *
 * @param[in] path  Path to validate.
 * @return          PG_OK if valid;
 *                  PG_ERR_INVALID_ARG if path is NULL;
 *                  PG_ERR_INVALID_PATH for empty/misordered commands,
 *                  unknown opcodes or non-finite coordinates.
 *
 * @note A MOVE-only path is structurally valid here; pg_measure_init()
 *       still rejects it as PG_ERR_DEGENERATE (no measurable length).
 */
pg_result_t pg_path_validate(const pg_path_t *path);

#ifdef __cplusplus
}
#endif

#endif /* PATH2D_PATH_H */
