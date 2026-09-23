# 原始需求（Original Requirements，§1–107）

> 本文件是 `lvgl-path-gauge` 原始任务书的收录版，使仓库规范自包含。
> 规范性修订见 [](task-book.md) §108–111；两文件冲突时以修订为准。
> 阶段状态：Phase 0–2 与 Phase 2.5/3 已完成，其余为后续阶段目标。

## 1–3 项目背景与范围

1. 背景：传统 `lv_arc` 难以描述非正圆、近似 Bézier 的仪表曲线（SOC 进度条、速度表刻度与指针轨道）；大量切图实现存在状态帧多、Flash 占用高、动画离散、UI 修改成本高、路径/刻度/指针缺乏统一几何模型等问题。
2. 两个层次：`path2d`（纯 C、嵌入式友好的二维路径几何与测量）→ `lv_path_gauge`（LVGL 9.1.0 任意路径仪表组件）。首期明确支持 LVGL 9.1.0 / C / MCU 与 RTOS / 运行期无 heap 依赖。
3. 仓库命名 `lvgl-path-gauge`；内部目录 `libs/path2d`、`src/lv_path_gauge`。项目定位一句话：轻量 C11 路径测量库 + 建立其上的 LVGL 任意曲线仪表组件。

## 4 许可证

- MIT。所有新增代码必须自主实现；不得复制无许可证第三方代码；可参考公开数学公式、W3C SVG 规范、Skia PathMeasure 等 API 思路；引入第三方 MIT 代码须保留声明；不得引入 GPL/AGPL（会阻断未来 LVGL upstream）。**强制要求。**

## 5 语言与兼容性

- C11；不得依赖 C++/STL/Eigen/Python runtime/渲染期 malloc/RT-Thread API/POSIX-only API；允许 `stdint.h` `stdbool.h` `stddef.h` `math.h`。
- LVGL adapter 才允许依赖 LVGL 9.1.0；`path2d` 必须能脱离 LVGL 在普通 host 上独立编译与测试。

## 6 目标平台

- 首个实际平台 ArtInChip D133/D211 + RT-Thread + LVGL 9.1.0 + 800×480；**核心库不得出现** ArtInChip/RT-Thread/D50T/CAN/车辆协议相关内容。

## 7 仓库结构

- 根：`CMakeLists.txt` `LICENSE` `README.md` `CHANGELOG.md`
- `libs/path2d/{include/path2d,src,tests}`：`pg_types.h` `pg_path.h` `pg_bezier.h` `pg_measure.h` `pg_flatten.h` 与对应 `.c`
- `include/lv_path_gauge.h`、`src/lv_path_gauge*.c`（Phase 4+）
- `examples/{basic_progress,segmented_soc,needle_speedometer}`、`tests/test_*.c`、`tools/svg2path/`、`docs/*`
- `lv_path_gauge_vector.c` 第一阶段可 stub，但文件与架构位置预留。

## 8 核心设计原则

- 8.1 Geometry 与 Renderer 分离：`pg_measure.c` 不得调用 `lv_draw_line()`。
- 8.2 Geometry 与 Gauge 分离：`path2d` 不得出现 SOC/speed/gauge/tick/needle/LVGL。
- 8.3 路径是唯一几何真相：track/progress/tick/needle/marker 全部由同一条 `pg_path_t` 派生，禁止手工维护 `needle_positions[]` 等平行表。
- 8.4 运行阶段零动态分配：`const path in Flash + caller-owned workspace + runtime zero heap allocation`。

## 9–10 数据模型与初始化宏

- `pg_cmd_type_t`：`PG_CMD_MOVE/LINE/QUAD/CUBIC/CLOSE`；`pg_point_t{x,y}`；`pg_cmd_t{type,p1,p2,p3}`；`pg_path_t{cmds,cmd_count}`，支持 `const` 静态路径置于 Flash。
- 语义：MOVE p1=target；LINE p1=endpoint；QUAD p1=control,p2=endpoint；CUBIC p1/p2=controls,p3=endpoint；CLOSE 无额外点。
- 初始化宏：`PG_MOVE_TO/LINE_TO/QUAD_TO/CUBIC_TO/CLOSE`，使设计稿转换结果可直接成为静态 C 数组。

## 11 Bézier 数学

- 必须实现 quad/cubic 的 `eval`、`derivative`、`split`（De Casteljau）；`t` 定义域 [0,1]，超范围输入需 clamp 或明确错误策略。

## 12–16 Path Measurement

- `pg_measure_sample_t{distance,t,command_index}`、`pg_measure_t{path,samples,sample_count,sample_capacity,total_length}`。
- `pg_measure_init(measure, path, workspace, workspace_count, tolerance)`：校验非法 path/workspace；建立 arc-length LUT；adaptive subdivision；返回 total length；**不使用 heap**；workspace 不足返回明确错误。
- 错误枚举：`PG_OK/PG_ERR_INVALID_ARG/PG_ERR_INVALID_PATH/PG_ERR_WORKSPACE_TOO_SMALL/PG_ERR_DEGENERATE`。
- Adaptive flatten：禁止固定 N=100 采样；使用 flatness estimator（control polygon 与 chord 的偏差）；曲率高自动更多 sample。
- Arc-length LUT：样本按 distance 递增记录；查询用二分（O(log N)）；禁止每次查询从起点线性积分。
- `pg_measure_get_pos_tan()` 与 `pg_measure_get_pos_tan_normalized()`（normalized ∈ [0,1]，0=起点、1=终点）。

## 17 LUT 只用于定位

- 不允许在 sample 之间线性插值 position；应由 LUT 得到近似 t，再对原始曲线做 evaluate；tangent 必须来自原曲线 derivative，保证低样本数下方向稳定。

## 18 Tangent 与 Normal

- 提供 `pg_vec_normalize()` 与 `pg_tangent_to_normal()`（默认 normal = `(-ty, tx)`）；文档必须说明坐标系：LVGL 屏幕 `+x` 右、`+y` 下，及 normal 正方向。

## 19 退化情形

- 必须处理零长 line、重合控制点、MOVE→MOVE、CLOSE 到同点、空 path；不得除零、NaN、Inf；所有 public API 行为明确。

## 20 多子路径

- 数据模型允许多 `MOVE`；`path2d` 不崩溃、measure 正确处理多 contour 或明确返回 unsupported；建议 measure 支持、gauge 限制 single open contour。

## 21–23 Slice、Writer、Flatten Writer

- `pg_measure_slice()` 支持任意 [start,end] 弧长截取；Bézier 截取必须保持 Bézier（De Casteljau），不得退化为折线。
- `pg_path_writer_t`：move_to/line_to/quad_to/cubic_to + ctx，供 slice 输出到 path buffer、LVGL Vector Path、测试收集器、调试 dump。
- `pg_flatten_cb` + `pg_path_flatten(path, tolerance, cb, ctx)` 供 line renderer 消费。

## 24–35 LVGL Widget（Phase 4+）

- `lv_path_gauge_create(parent)` 继承 `lv_obj`；内部持有 `const pg_path_t*` 与 `pg_measure_t`、min/max/value。
- V1 范围：track（色/宽/透明度）、progress（min→value 连续 stroke）、zones（固定上限，例如 `LV_PATH_GAUGE_MAX_ZONES 8`）、needle（沿路径移动，V1 程序绘制三角/楔形，API 允许未来 image needle）、ticks（等值或手动 values，inside/outside/centered）、tick label 位置 helper（`lv_path_gauge_get_value_geometry()`）。
- needle 几何来自 tangent/normal（`length/width/offset_normal/color`）；角度必须来自局部 tangent，禁止 `needle_angle_table[]`。
- value API：`set_range/get_value/set_value/set_value_anim`；只负责 value，禁止 CAN/协议/SOC 滤波/车辆模型。
- 第一 renderer 必须用 `lv_draw_line()`（`LV_EVENT_DRAW_MAIN`），不要用 N 个 `lv_line` widget；正确处理线宽/圆头/接缝。
- 禁止每帧重新 flatten/measure：`set_path()` 才允许重建；`set_value()` 只更新 value + invalidate。
- Path cache 固定容量（如 `LV_PATH_GAUGE_MAX_FLAT_POINTS 128`）；vector backend 预留枚举。

## 36–43 SVG 工具与示例

- `tools/svg2path/`（host-side，允许 Python）把 SVG path 归一化为 M/L/Q/C/Z 并生成 `pg_cmd_t`/`pg_path_t` C 源码；设备端绝不解析 SVG/XML；SVG arc 在 host 侧转 cubic。
- 示例：basic progress（任意 S 曲线 0–100%）、SOC gauge（非圆路径 + 红/橙/绿分区 + 0→100→0 动画）、needle speedometer（0–25，刻度 0/5/10/15/20/25，指针沿路径 + 自动旋转）。

## 44–50 测试要求

- Line `(0,0)→(100,0)`：长度 100、50% 位置 `(50,0)`、tangent `(1,0)`；垂直线；折线累计长度；quad/cubic 的 t=0/0.5/1；split 连续性（left end == right start）与重构误差。
- Path length 使用高精度参考（允许 host 用 100000 samples 作 oracle），生产误差门槛初始 `< 0.2%` 或等价的 tolerance 倍数，需在 README 说明。
- Pos-at-distance：0/10/25/50/75/100% 与高精度 reference 对比。
- Tangent：`|tangent| ≈ 1` 并与数值微分方向一致。
- 退化测试全集：零 line、重复控制点、极小曲线、空 path、MOVE only、重复 MOVE、CLOSE；保证无崩溃、无 NaN/Inf。
- Workspace exhaustion 必须返回 `PG_ERR_WORKSPACE_TOO_SMALL`，禁止静默截断。
- Gauge 边界值 `value < min / == min / == max / > max` 统一 clamp。

## 51–56 性能、内存与配置

- Host benchmark 至少统计 measure_init 时间、get_pos_tan 1000/10000/100000 次查询、flatten 点数、workspace 字节数；目标板后续记录 setup/draw 时间、CPU、FPS、RAM、Flash（32/64/96/128 点对比）。
- 运行期查询 = binary search + 1 次 curve eval + 1 次 derivative，不做递归 subdivision；一个 SOC path + 一个 speed path + 动态 needle 不得成为 LVGL 主线程明显瓶颈。
- 内存固定可预测：measure samples ≤ 128、flatten points ≤ 128、zones ≤ 8、ticks ≤ 16（第一版 compile-time configurable），禁止每 frame malloc。
- 配置宏：`PG_MAX_RECURSION`（自适应递归上限，防栈失控）、`LV_PATH_GAUGE_MAX_ZONES`、`LV_PATH_GAUGE_MAX_TICKS`。

## 57–59 代码风格与 API 边界

- 贴近 LVGL 风格：snake_case、`lv_`/`pg_` 前缀、4 空格缩进；public 符号只能是 `pg_*` / `lv_path_gauge_*`；禁止 `path_init()`/`draw()` 这类泛化命名。
- public（`include/`）与 private（`src/*_private.h`）分离，应用不得依赖 private header（利于 upstream）。
- 文档要求：README 解释项目存在原因、为什么 `lv_arc` 不适合任意路径、architecture、build、quick start、examples、memory model、LVGL version、limitations。

## 60–65 Architecture 与 Upstream 计划

- `docs/architecture.md` 展示 `SVG → pg_path → pg_measure → renderer → lv_path_gauge` 与 `geometry ≠ renderer ≠ widget`。
- `docs/upstream-plan.md` 记录未来可能提交 LVGL 主线的重点：**Path Measurement API**（length / point at distance / tangent / segment extraction），而不是 Gauge widget；映射如 `pg_measure_get_pos_tan` → `lv_vector_path_measure_get_pos_tan`，但当前产品 API 不被未来 LVGL API 假设绑死。
- 当前 LVGL master 已有 `lv_vector_path_move_to/line_to/quad_to/cubic_to/arc_to/close` 与 bounding box，缺少公开的 path length / point at distance / tangent / segment extraction —— 这是 upstream 切入点。
- 不允许在 LVGL 9.1 fork 内为 upstream 直接开发；upstream 工作基于 LVGL master 独立 prototype；D50T 使用 9.1.0，两者相互独立。

## 66–76 开发阶段划分

- Phase 0 Repository Bootstrap：repo、MIT、README、CMake、CI、basic test（验收：configure/build/ctest 全绿）。**已完成**
- Phase 1 Bézier Core：Line/Quad/Cubic 的 eval/derivative/split + unit tests（不写 Gauge）。**已完成**
- Phase 2 Path + Measure：pg_path、adaptive flatten、arc-length LUT、binary search、get_length、get_pos_tan。**已完成**
- Phase 3 Slice：path segment extraction（0–25% / 20–80% / 50–100%）+ writer。**已完成**
- Phase 4 LVGL Minimal Gauge（single path + track + progress，单色）；Phase 5 SOC Gauge（zones）；Phase 6 Needle Gauge（ticks + needle + tangent orientation）；Phase 7 SVG Conversion Tool；Phase 8 Optimization（static cache、dirty redraw、allocation review、profiling）；Phase 9 Vector Renderer（LVGL Vector/ThorVG A/B）；Phase 10 Upstream Readiness（API、benchmark、覆盖率、limitations，针对 master 建 prototype）。

## 77–84 开发流程、CI 与 Non-goals

- 分支建议 `main` + `dev/path2d-core`、`dev/measure`、`dev/lvgl-gauge`、`dev/svg-tool`、`dev/vector-backend`；commit 建议 Conventional/LVGL 风格（`feat(path2d): ...`），每个 commit 保持可编译。
- CI：至少 Ubuntu GCC + Clang，`-Wall -Wextra -Wpedantic`，推荐 ASan/UBSan（host only），至少一套配置 `-Werror`（第三方头导致的 warning 可只对 `path2d` 启用）。
- 建议尽早建立 clang-format / clang-tidy（非第一阶段阻塞）；debug helper（`pg_debug_dump_path/measure`）必须在 `PG_ENABLE_DEBUG` 下。
- V1 Non-goals：NURBS、B-spline、Catmull-Rom、3D、path boolean、offset curve、stroke tessellation engine、SVG runtime parsing/rendering、font-on-path、CAN/CANopen、RT-Thread 集成、完整设计编辑器、gradient path stroke、GPU backend。不做“万能绘图库”，严格控制 scope。

## 85–94 验收、精度、更新效率与浮点

- 产品验收案例 1（SOC）：range 0–100，zones 0–20 红 / 20–40 橙 / 40–100 绿，非圆开放 Bézier；验收值 0/10/20/30/40/50/75/100。
- 动画验收：0→100、100→0、20→80、80→35，不得断层/倒退/颜色错序/路径跳变。
- 产品验收案例 2（Speed）：0–25 km/h，ticks 0/5/10/15/20/25，needle 0/2.5/…/25 连续运动；任意位置角度来自局部 tangent/normal。
- 视觉精度：800×480 下路径相对设计稿误差推荐 < 1px（SOC 外缘、指针轨迹、刻度位置）。
- Gauge 更新效率：`set_value()` 不得重新 measure/flatten；`set_path()` 才允许重建。
- API 生命周期：`pg_path_t`、workspace、gauge object 归属明确；`lv_path_gauge_set_path()` 默认不复制 path，path 必须存活于 gauge 生命周期内。
- 线程安全：不要求内部 thread-safe；README 明确 LVGL API 必须在 LVGL 线程调用；`path2d` 只读查询在不同对象间可 reentrant，但不提供同步。
- 浮点策略：V1 统一 `float`，不提前实现 fixed-point；但不写 `double` 字面量导致无意双精度（用 `0.5f`/`1.0f`）。

## 95–107 数值稳健、工具、方法与红线

- `PG_EPSILON` 统一所有几何判断，禁止散落 `1e-5`/`0.0001`。
- 建议 host 生成 SVG debug 输出（original path/samples/tangent/normal/slice）辅助排查，但不进 embedded target。
- README Quick Start 目标使用复杂度：定义静态 path → create → set_path → set_range → set_value。
- D50T 专用示例可存在，但核心库不得 hardcode D50T。
- Agent 开发要求：先读完整任务书；不擅自扩大 scope；每个 Phase 先跑测试；测试不过不得堆后续功能；API 重大变化更新文档；不复制许可不明代码；不为快速 demo 把 geometry 写进 widget；不为 upstream 破坏独立边界；正确性 > 架构 > 性能；优化必须有 benchmark 依据。
- 每 Phase 输出报告：Completed / Files changed / API added / Tests / Known limitations / Memory & performance / Next phase。
- 第一轮停止点：只做 Phase 0–2（验收目标：`pg_measure_get_pos_tan_normalized(&measure, 0.53f, &pos, &tangent)` 稳定、准确、无 heap）；第二轮 Phase 3–6；第三轮 SVG tool / vector backend / embedded profiling / upstream prototype。
- 最终定义：面向嵌入式系统的轻量 C11 二维路径测量库，以及建立其上的 LVGL 任意曲线仪表组件。
- 架构红线（始终遵守）：`Path ≠ Gauge`、`Geometry ≠ Rendering`、`Rendering ≠ Vehicle Logic`；依赖只能是 `Application → lv_path_gauge → path2d`，绝不能出现 `path2d → LVGL` 或 `path2d → D50T`。
