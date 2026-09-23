# Task Book（任务书）

> `docs/requirements.md` 收录原始任务书 §1–107（本项目规范的自包含主体）；
> 本文件记录其后的规范性修订 §108–111。两者共同构成 `lvgl-path-gauge` 的
> 约束性任务书。

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
*   **魔法数字**：标准数学公式中的自然系数（De Casteljau 的 `0.5f`、Bernstein 基的 `2.0f`/`3.0f`、参数区间的 `1.0f` 等）**可以**直接使用，无需命名。禁止的是经验性、调参性或具有物理意义的常量匿名出现：容差、阈值、容量、上限、递归深度等必须用 `#define`/`enum` 命名，并注明语义与单位（如 `#define PG_FLATNESS_TOLERANCE 0.25f /* px */`）。

#### 111. 结构体字段注释 (Struct Field Comments)

所有 `typedef struct` 的字段必须使用 Doxygen 的 `<` 语法或行尾注释进行说明，特别是涉及内存所有权和单位的字段。

```c
typedef struct {
    const pg_cmd_t *cmds;     /**< Pointer to path commands (Must be kept alive by caller). */
    uint16_t cmd_count;       /**< Total number of commands. */
} pg_path_t;
```
