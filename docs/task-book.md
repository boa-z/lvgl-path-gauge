# Task Book（任务书）

> `docs/requirements.md` 收录原始任务书 §1–107（本项目规范的自包含主体）；
> 本文件记录其后的规范性修订 §108–117。两者共同构成 `lvgl-path-gauge` 的
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

#### 112. Flatten 输出契约 (Flatten Writer Contract)

- `pg_path_flatten(path, tolerance, writer)` 输出 `move_to`（每个 MOVE command）+ `line_to`（每个平坦叶弦）。
- 禁止把裸点流交给 renderer：contour 边界必须由 `move_to` 表达。
- writer 失败立即中止并原样传播（如 `PG_ERR_WORKSPACE_TOO_SMALL`），不得静默截断。
- flatten 与 measure 必须共用同一 subdivision/flatness 实现。

#### 113. Slice 拓扑 (Slice Topology)

- contour 边界只能来自原始 MOVE command；禁止用坐标比较推断 subpath break。
- `M(0,0) L(10,0) M(10,0) L(20,0)` 的全量 slice 必须输出 MOVE/LINE/MOVE/LINE。
- distance 落在 joint 上解析为后继 command 的 `t = 0`（既有语义不变）。

#### 114. Tangent 回退顺序 (Tangent Fallback)

1. 曲线解析导数（非零）；
2. 相邻可测 LUT span 的局部方向（相邻采样点连线，按距离递增方向）；
3. 整条 command 的弦方向；
4. `(1, 0)`。

- 必须覆盖端部退化柄：`M(0,0) C(0,0, 0,100, 100,100)` 起点 tangent 约为 `(0,1)`；末端退化柄约为 `(1,0)`；不得退化为 `(1,1)` 对角线。

#### 115. pg_path_buffer_t 加固

- 任何 writer 失败（容量、缺 MOVE、非有限坐标、NULL storage）都使 buffer 进入 poisoned 状态并记录首个失败码；此后所有写入以该码失败，且不可导出。
- `pg_path_buffer_init()` 返回 `pg_result_t`；NULL storage / 零容量安全失败（不崩溃）。
- `pg_path_buffer_writer(NULL)` 返回空 writer（回调全 NULL），消费者以 `PG_ERR_INVALID_ARG` 拒绝。
- `pg_path_buffer_to_path()` 失败时清零输出并返回原始失败码。

#### 116. CMake 规范

- `PATH2D_STRICT_WARNINGS`：standalone 默认 ON，subproject 默认 OFF；只作用于本项目 target。
- 必须提供 `path2d::path2d` alias（以及 `lv_path_gauge::lv_path_gauge`）。
- CI 保持 GCC/Clang + ASan/UBSan；LVGL 集成作业使用精确固定的 `v9.1.0`。

#### 117. LVGL Gauge 契约（Phase 4 v1）

- `lv_path_gauge` 是 `lv_obj_class_t` 自定义 widget；只用 LVGL public API，不 include private header。
- 绘制路径：`LV_EVENT_DRAW_MAIN` → `lv_event_get_layer` → `lv_draw_line_dsc_init` → `lv_obj_init_draw_line_dsc` → `lv_draw_line`（对齐 LVGL 9.1 自身 `lv_line`）。
- `LV_PART_MAIN` = track；`LV_PART_INDICATOR` = active progress（单色，v1）。
- Path 坐标为 object-local 像素，v1 不自动 scale/fit；`LV_EVENT_GET_SELF_SIZE` 提供 bbox。
- `LV_EVENT_REFR_EXT_DRAW_SIZE` 必须覆盖线宽与越界路径点：粗线/越界路径不得因 invalid area 被裁剪。
- `set_path()` 只接受 single open contour（恰一个 MOVE、无 CLOSE、有限坐标）；内部一次 `pg_measure_init` + 一次 flatten 到 caller-owned render cache，并记录每个 vertex 的 cumulative distance。
- `lv_path_gauge_workspace_t` 由调用方持有（measure sample 区 + 顶点区 + 距离区）；除 LVGL object 分配外，不得为 path/cache 额外 malloc。
- `set_value()` 只 clamp/store/invalidate：禁止 measure/flatten/cache 重建；progress 直接从缓存折线按 distance range 裁切（禁止每帧 `pg_measure_slice()`+flatten）。
- range 默认 0..100、value 默认 0；`max <= min` 拒绝；int32 极值范围用 int64 中间量，不得溢出。
- v1 非目标：zones、SOC 三色、ticks、needle、label、animation convenience API、Vector/ThorVG、SVG。

#### 118. Production API Freeze（Phase 4.5）

- `lv_path_gauge_workspace_t` 只保存 caller 提供的指针与容量（samples / vertices / distances），禁止内嵌定长数组；结构布局不得随任何容量宏变化（ABI 稳定）。`LV_PATH_GAUGE_MAX_SAMPLES` / `LV_PATH_GAUGE_MAX_VERTICES` 仅作为推荐容量。
- `pg_measure_t`、vertex count、total distance 等运行时元数据属于 private `lv_path_gauge_t`；`set_path()` 对 workspace 只读，不回写。
- 移除 public `lv_path_gauge_get_vertex_*()` 渲染缓存内省 API；`lv_path_gauge_get_total_distance()` 亦不再 public，render cache/polyline length 保持 private/debug。
- `lv_path_gauge_clear_path()` 承担清理语义；`set_path()` 不接受 NULL path（`PG_ERR_INVALID_ARG`，且与其他失败一样不留旧几何）。
- `lv_path_gauge_workspace_init()` 接收指针+容量、返回 `pg_result_t`、fail-atomic（失败清零描述符，不触碰数组）。
- NaN/Inf tolerance 在 path2d 与 gauge 的全部入口统一拒绝（`PG_ERR_INVALID_ARG`）；`tolerance <= 0` 仍选择默认容差。
- draw-path 禁止 double division（value fraction 使用 int64 中间量与单次 float 除法）；保持 zero-extra-heap。

#### 119. Segmented zones（Phase 5）

- 固定容量 `LV_PATH_GAUGE_MAX_ZONES`（默认 8）存于 widget 实例内，不占 caller 存储、不改变任何 public 结构体布局。
- zone = 半开值域 `[start, end)` + `lv_color_t`（start 含、end 不含）；边界值属于后继 zone，但该值处后继 zone 与 active progress 的交集为空（可见长度为 0），视觉上仅绘制前区间——不得表述为“边界值属于前一区间”。`set_zones()` 原子：先全量校验后复制；要求 start 升序、不得 overlap、允许 gap、`start < end`；count 超容量返回 `PG_ERR_WORKSPACE_TOO_SMALL`；`clear_zones()` 清除，count == 0 等价清除。
- gap 使用 `LV_PART_INDICATOR` 基础颜色；zone 只覆盖颜色，线宽/opa/rounded 仍来自 `LV_PART_INDICATOR`；zone 超出当前 range 时按 intersection 绘制，不修改配置。
- 渲染：active progress 划分为 base/zone 子段；不得为 zone 创建 LVGL object、不得重建 path geometry；round cap 仅作用于整个 active run 的外起点与真正终点，zone boundary 一律平头。
- `set_value()` 仍只 clamp/store/invalidate。

#### 120. Stroke continuity（Phase 5.1）

- 明确区分 geometry joint 与 semantic boundary：同一 continuous colour run 内部的所有 polyline joint 必须无缝——采用 LVGL round cap 的同色圆盘 filler（`round_end=1`，LVGL 在端点绘制直径=线宽的实心圆）或等价的显式 filler，禁止 background/track 透缝。
- 只有整个 stroke/progress 的真正首尾按 `line_rounded` 使用 round cap；zone 边界必须平头，不得出现两个颜色圆帽重叠。
- `LV_PART_MAIN` track 与 `LV_PART_INDICATOR` progress 使用同一 seamless joint 逻辑（joint 无缝与 `line_rounded` 无关）。
- 禁止用降低 tolerance/增加采样掩盖接缝；回归测试必须用高曲率、20–30px 粗线在 memory display 上逐 cached joint 检查不得出现 background-coloured hole，并检查 zone 边界无 background seam、无颜色反序。
- 若 butt zone 边界仍因 LVGL 9.1 软件光栅器出现 ≤1px AA 缝，可增加明确命名、最多约 1px、不随线宽缩放的局部 overlap 补偿。
- `lv_path_gauge_get_total_distance()` 不再 public；render cache/polyline length 保持 private/debug。
