# Changelog

All notable changes to this project will be documented in this file.

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
  caller-owned command buffer with overflow flag and `to_path()` view.
- `pg_measure_slice()` / `pg_measure_slice_normalized()`: arc-length range
  extraction across commands and subpaths; curves keep their original degree
  through De Casteljau range extraction; subpath jumps emit `move_to`.
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
