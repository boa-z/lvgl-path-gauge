# Task Book（任务书）

> Sections 1–107: original project brief (2026-09-24), kept with the
> project owner. This file records the normative amendments accepted
> afterwards. Together they form the binding task book for `lvgl-path-gauge`.

---

#### 108. 文件头规范 (File Headers)

所有 `.c` 和 `.h` 文件必须包含标准文件头。禁止遗漏。

```c
/**
 * @file pg_bezier.h
 * @brief Bézier curve evaluation and subdivision primitives.
 *
 * Copyright (c) 2024 [Your Name/Organization]
 * SPDX-License-Identifier: MIT
 */
```

#### 109. 强制 Doxygen API 注释 (Public API Documentation)

`include/path2d/` 和 `include/lv_path_gauge.h` 中的所有 public 函数和结构体，必须使用 Doxygen 格式注释。
AI Agent 生成的公开 API 必须包含以下要素，否则视为不合格：

```c
/**
 * @brief Calculates the exact position and tangent on the path at a given normalized distance.
 *
 * @param[in]  measure   Pointer to the initialized measure workspace.
 * @param[in]  normalized Normalized distance along the path [0.0, 1.0].
 * @param[out] position  Output point coordinate. Cannot be NULL.
 * @param[out] tangent   Output normalized tangent vector. Can be NULL if not needed.
 * @return               PG_OK on success, PG_ERR_INVALID_ARG if measure or position is NULL.
 *
 * @note The tangent vector is normalized (length = 1.0).
 *       In LVGL's coordinate system (+Y is down), the positive normal points to the
 *       "right" side of the path's forward direction.
 */
pg_result_t pg_measure_get_pos_tan_normalized(
    const pg_measure_t *measure,
    float normalized,
    pg_point_t *position,
    pg_point_t *tangent
);
```

#### 110. 内部实现注释 (Internal Implementation Comments)

*   **禁止废话注释**：禁止 `// increment i` 这种毫无意义的注释。
*   **数学公式溯源**：涉及几何算法（如 De Casteljau, Flatness estimator, Arc-length LUT 构建）时，必须用注释标明算法来源或数学推导逻辑。
    *   *示例*：`/* Flatness estimator: calculate max distance from control points to the chord (p0-p3). See SVG spec appendix. */`
*   **魔法数字清零**：除了 `0` 和 `1`，代码中禁止出现未命名的浮点常量。必须使用 `#define` 或 `enum`，并附带注释说明物理意义（如 `#define PG_FLATNESS_TOLERANCE 0.25f /* px squared */`）。

#### 111. 结构体字段注释 (Struct Field Comments)

所有 `typedef struct` 的字段必须使用 Doxygen 的 `<` 语法或行尾注释进行说明，特别是涉及内存所有权和单位的字段。

```c
typedef struct {
    const pg_cmd_t *cmds;     /**< Pointer to path commands (Must be kept alive by caller). */
    uint16_t cmd_count;       /**< Total number of commands. */
} pg_path_t;
```
