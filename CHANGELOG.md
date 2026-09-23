# Changelog

All notable changes to this project will be documented in this file.

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
