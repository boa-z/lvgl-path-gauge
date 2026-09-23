# Changelog

All notable changes to this project will be documented in this file.

## [0.3.0] - 2026-09-24

### Added (Phase 4: LVGL 9.1 minimal gauge)

- `lv_path_gauge`: real `lv_obj_class_t` widget drawing a static track
  (`LV_PART_MAIN`) and a single-colour progress (`LV_PART_INDICATOR`) along a
  single open contour. LVGL 9.1.0, public API only, drawing mirrors
  `lv_line` (`LV_EVENT_DRAW_MAIN` -> `lv_event_get_layer` ->
  `lv_draw_line_dsc_init` -> `lv_obj_init_draw_line_dsc` -> `lv_draw_line`).
- `lv_path_gauge_workspace_t`: caller-owned storage for the arc-length LUT and
  the flattened render cache (vertices + cumulative distances). No heap for
  path data beyond LVGL's own object allocation.
- `lv_path_gauge_set_path()` measures and flattens exactly once, validates
  topology (exactly one MOVE, no CLOSE, finite coordinates) and leaves no
  stale geometry on failure.
- `lv_path_gauge_set_value()` clamps, stores and invalidates only; progress is
  cut out of the cached polyline by distance, never by re-measuring.
- `LV_EVENT_REFR_EXT_DRAW_SIZE` covers the stroke width and path points
  outside the object box, so thick strokes are not clipped;
  `LV_EVENT_GET_SELF_SIZE` reports the path bounding box.
- Range/value guards: default 0..100/0, `max <= min` rejected, int64
  intermediates for extreme int32 ranges.
- Host LVGL integration tests: state machine/API coverage and an 800x480
  memory-display draw smoke test (0/25/50/100%, colour counts, clip check).
- `examples/basic_progress`: non-circular S curve animated 0 -> 100 -> 0 on a
  memory display with PPM snapshots and a deterministic `--smoke` mode.
- Optional SDL2 window front-end for the example
  (`-DLV_PATH_GAUGE_SDL=ON`, `--window [--frames N]`) so the animation can be
  watched on PC; the widget itself has no SDL dependency and CI runs the
  windowed example headlessly with `SDL_VIDEODRIVER=dummy`.
- CI job building the widget, tests and example against the pinned LVGL
  v9.1.0 (GCC/Clang, plain and ASan+UBSan).

### Added (Phase 3.5: contracts, topology and buffer hardening)

- `pg_path_flatten()` now emits `move_to`/`line_to` through
  `pg_path_writer_t`: contour boundaries survive into renderers and sink
  errors (e.g. `PG_ERR_WORKSPACE_TOO_SMALL`) abort immediately.
- `pg_measure_slice()` keeps contour topology from the original MOVE
  commands; a same-coordinate subpath break
  (`M(0,0) L(10,0) M(10,0) L(20,0)`) now slices as MOVE/LINE/MOVE/LINE.
- Tangent fallback tiers: analytic derivative, then the local direction of
  the adjacent measurable LUT span, then the command chord, then `(1, 0)`.
  Collapsed end handles (`M(0,0) C(0,0, 0,100, 100,100)` and its mirror)
  report the true travel direction instead of the `(1,1)` diagonal.
- `pg_path_buffer_t` hardening: any writer failure poisons the buffer with
  the original code (sticky), `pg_path_buffer_init()` returns `pg_result_t`,
  NULL storage/zero capacity fail safely, `pg_path_buffer_writer(NULL)`
  returns an empty writer, and `pg_path_buffer_to_path()` clears its output
  on failure.
- CMake: `PATH2D_STRICT_WARNINGS` (standalone ON / subproject OFF) and the
  `path2d::path2d` alias.

## [0.2.0] - 2026-09-24

### Fixed (Phase 2.5 geometry hardening)

- Adaptive flatness is now a shared engine (`src/pg_subdiv.c`) used by both
  flattening and measurement; the duplicated per-module algorithms are gone.
- Collinear overshoot/backtracking curves are measured correctly: flatness
  requires perpendicular deviation **and** control-polygon excess to be within
  tolerance. `M(0,0) Q(100,0) (10,0)` now reports the true arc length
  (~95.2632) instead of the chord (10); dense-oracle regressions cover quad
  and cubic cases, and monotone collinear spans still flatten in one step.
- `pg_path_validate()` rejects NaN/Inf in every coordinate an opcode consumes
  (unused slots stay unchecked).
- `pg_measure_init()` is fail-atomic: any failure zeroes the object.
- Zero-length prefix commands no longer define the start tangent: the LUT
  anchors on the first measurable span.
- `pg_measure_get_pos_tan[_normalized]()` accept `tangent == NULL`.
- Docs no longer claim an "exact" arc-length position: the parameter is
  LUT-interpolated, the position comes from evaluating the original curve.
- Removed the ineffective `PG_USE_FLOAT` switch.
- CMake flags are target-scoped (`path2d_dev_flags` interface library); tests
  default to OFF when consumed as a subproject.

### Added (Phase 3 writer and slice)

- `pg_path_writer_t`: move/line/quad/cubic sinks that return `pg_result_t` and
  propagate `PG_ERR_WORKSPACE_TOO_SMALL` without silently truncating.
- `pg_path_buffer_t` + `pg_path_buffer_writer()`: fixed-capacity,
  caller-owned command buffer with `to_path()` view.
- `pg_measure_slice()` / `pg_measure_slice_normalized()`: arc-length range
  extraction across commands and subpaths; curves keep their original degree
  through De Casteljau range extraction.
- Slice contract: NaN or `start > end` -> `PG_ERR_INVALID_ARG`, out-of-range
  clamped, `start == end` -> a single MOVE, writer errors propagated.
- `docs/requirements.md` (original requirements §1–107) makes the repository
  self-contained; `docs/task-book.md` §110 now allows natural mathematical
  coefficients while still banning anonymous tuning/threshold constants.

### Tests

- New `tests/test_writer.c` and `tests/test_slice.c`; extended
  bezier/path/flatten/measure coverage (collinear regressions, NaN/Inf
  validation, fail-atomic, zero-length prefix tangent, NULL tangent,
  multi-subpath joints, workspace exhaustion).
- Benchmarks report LUT build time, query throughput and slice throughput.

## [0.1.0] - 2026-09-24

### Added (Phase 0-2)

- Repository bootstrap: MIT license, CMake build, GitHub Actions CI
  (GCC/Clang, `-Wall -Wextra -Wpedantic -Werror`, ASan, UBSan).
- `path2d` core library (pure C11, no LVGL/RTOS/heap in the runtime path):
  - `LINE` / `QUAD` / `CUBIC` path commands (`MOVE`, `CLOSE` supported).
  - Bezier evaluate / derivative / split (De Casteljau).
  - Adaptive flatten with bounded recursion.
  - Arc-length LUT measurement with binary-search distance mapping.
  - `pg_measure_get_pos_tan()` and
    `pg_measure_get_pos_tan_normalized()`.
- Host test suite (CTest): bezier, path, flatten, measure + benchmark.
- `docs/architecture.md`.
