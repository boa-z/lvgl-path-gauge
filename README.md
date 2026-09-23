# lvgl-path-gauge

> A lightweight C11 path measurement library and arbitrary-path gauge widget
> for LVGL, designed for embedded systems and non-circular instrument clusters.

LVGL `lv_arc` / `lv_scale` target circular geometry. Real instrument clusters
(SOC status bars, needle speedometers on free-form curves) need one shared
geometric truth: a 2D path from which track, progress, zones, ticks and the
needle are all derived. This repository builds that foundation first
(`path2d`), then the LVGL widget (`lv_path_gauge`) on top of it.

```text
Application
      |
lv_path_gauge        (Phase 4+, LVGL 9.1.0 widget)
      |
path2d               (Phase 0-2, pure C11, no LVGL/RTOS/heap)
```

Current status: **Phase 0-2 complete** — repository, `path2d` core
(LINE/QUAD/CUBIC, evaluate/derivative/split, adaptive flatten, arc-length
LUT, `pg_measure_get_pos_tan[_normalized]()`) and the host test suite.
No LVGL widget yet (starts Phase 4, after review).

## Why not `lv_arc`?

- `lv_arc` describes circles; SOC/speed gauges here follow non-circular,
  Bezier-like open curves.
- Cut-frame animation costs Flash, looks discrete and is expensive to restyle.
- Path, ticks and needle positions need one shared geometric model so a
  redesign or a resolution change does not require re-cutting assets.

## Repository layout

```text
libs/path2d/{include,src,tests}  dependency-free geometry core
tests/                           host tests (CTest) + benchmark
docs/architecture.md             geometry != renderer != widget
```

Phase 4+ will add `src/lv_path_gauge*`, `examples/` and `tools/svg2path/`.

## Build and test (host)

Requires CMake 3.16+ and a C11 compiler (GCC or Clang).

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Strict warnings are always on (`-Wall -Wextra -Wpedantic -Werror`).
Sanitizer run (host only):

```bash
cmake -S . -B build-asan -DPATH2D_SANITIZE=ON
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

CI (`.github/workflows/ci.yml`) runs GCC + Clang, plain and
`address,undefined` sanitizers, with `UBSAN_OPTIONS=halt_on_error=1`.

## Quick start

```c
#include "path2d/pg_path.h"
#include "path2d/pg_measure.h"

static const pg_cmd_t path_cmds[] = {
    PG_MOVE_TO(0.0f, 0.0f),
    PG_CUBIC_TO(0.0f, 100.0f, 100.0f, 100.0f, 100.0f, 0.0f),
};
static const pg_path_t path = { path_cmds, PG_ARRAY_SIZE(path_cmds) };

static pg_measure_sample_t workspace[128];
pg_measure_t m;

if (pg_measure_init(&m, &path, workspace, 128, 0.5f) != PG_OK) {
    /* handle PG_ERR_WORKSPACE_TOO_SMALL / PG_ERR_DEGENERATE / ... */
}

pg_point_t pos, tan;
pg_measure_get_pos_tan_normalized(&m, 0.53f, &pos, &tan);
```

## Memory model

- Paths are `const` and live in Flash.
- Measurement workspace is caller-owned (`pg_measure_sample_t array[...]`).
- The runtime path (init once, query many times) performs **zero heap
  allocation**. A too-small workspace returns
  `PG_ERR_WORKSPACE_TOO_SMALL` instead of silently truncating geometry.
- Rule of thumb: 128 samples cover typical instrument paths at 0.5-unit
  tolerance; recursion is capped by `PG_MAX_RECURSION` (default 12).

## Coordinates and conventions

- Screen coordinates: `+x` right, `+y` down.
- `t` (curve parameter) is always clamped to `[0, 1]`.
- `normalized` queries clamp to `[0, 1]`: `0` -> path start, `1` -> path end.
- `pg_tangent_to_normal()` returns `(-t.y, t.x)`.
- All public APIs define degenerate behavior (zero-length segments,
  repeated control points, empty/MOVE-only paths): no crash, no NaN/Inf,
  explicit `pg_result_t` codes.

## Floating point

v1 uses `float` with `f`-suffixed literals throughout. All geometric
predicates share `PG_EPSILON`. No fixed-point until benchmarks demand it.

## Limitations (v1)

- Single-precision only; no NURBS/B-spline/Catmull-Rom, no 3D.
- `pg_measure_init()` flattens whole commands (no partial-path slicing yet;
  Phase 3 adds slice + writer abstraction).
- Multi-subpath (`MOVE...MOVE...`) paths measure one continuous distance;
  the future gauge accepts a single open contour.
- Threading: objects are reentrant for concurrent read-only queries on
  separate objects; no internal synchronization. LVGL APIs must run on the
  LVGL thread.

## License

MIT, see [LICENSE](LICENSE). All code is original implementation; do not
introduce GPL/AGPL material (it would block future LVGL upstream work).
