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
path2d               (pure C11, no LVGL/RTOS/heap)
```

Current status: **Phase 0-3 complete** — repository, `path2d` core
(LINE/QUAD/CUBIC, evaluate/derivative/split, shared adaptive flatten,
arc-length LUT, `pg_measure_get_pos_tan[_normalized]()`), Phase 2.5 geometry
hardening and Phase 3 (`pg_path_writer_t`, fixed-capacity path buffer,
`pg_measure_slice[_normalized]()`). No LVGL widget yet (Phase 4, after
review).

Normative documents: [original requirements](docs/requirements.md) §1–107 and
[task book amendments](docs/task-book.md) §108–111.

## Why not `lv_arc`?

- `lv_arc` describes circles; SOC/speed gauges here follow non-circular,
  Bezier-like open curves.
- Cut-frame animation costs Flash, looks discrete and is expensive to restyle.
- Path, ticks and needle positions need one shared geometric model so a
  redesign or a resolution change does not require re-cutting assets.

## Repository layout

```text
libs/path2d/include/path2d/  pg_types, pg_path, pg_bezier, pg_flatten,
                             pg_measure, pg_writer
libs/path2d/src/             pg_path, pg_bezier, pg_subdiv (shared
                             subdivision), pg_writer, pg_slice, pg_flatten,
                             pg_measure
tests/                       host tests (CTest) + benchmark
docs/requirements.md         original requirements §1-107
docs/task-book.md            amendments §108-111 (headers, doxygen, comments)
docs/architecture.md         geometry != renderer != widget
```

Phase 4+ will add `src/lv_path_gauge*`, `examples/` and `tools/svg2path/`.

## Build and test (host)

Requires CMake 3.16+ and a C11 compiler (GCC or Clang).

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Strict warnings are target-scoped (`path2d_dev_flags`): `-Wall -Wextra
-Wpedantic -Werror` for this project's targets only, so downstream consumers
never inherit them. Sanitizer run (host only):

```bash
cmake -S . -B build-asan -DPATH2D_SANITIZE=ON
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

`PATH2D_BUILD_TESTS` defaults to ON for standalone builds and OFF when this
repository is included as a subproject. CI (`.github/workflows/ci.yml`) runs
GCC + Clang, plain and `address,undefined` sanitizers, with
`UBSAN_OPTIONS=halt_on_error=1`.

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
    /* PG_ERR_WORKSPACE_TOO_SMALL / PG_ERR_DEGENERATE / ... */
}

pg_point_t pos, tan;
pg_measure_get_pos_tan_normalized(&m, 0.53f, &pos, &tan);
/* tan may be NULL when only the position is needed. */
```

Extracting a sub-range keeps the original curve degree:

```c
#include "path2d/pg_writer.h"

static pg_cmd_t slice_cmds[16];
pg_path_buffer_t buffer;
pg_path_t slice;

pg_path_buffer_init(&buffer, slice_cmds, PG_ARRAY_SIZE(slice_cmds));
pg_path_writer_t writer = pg_path_buffer_writer(&buffer);

if (pg_measure_slice_normalized(&m, 0.2f, 0.8f, &writer) != PG_OK) {
    /* writer overflow: the slice is incomplete, buffer.overflowed is set */
}
pg_path_buffer_to_path(&buffer, &slice); /* PG_ERR_WORKSPACE_TOO_SMALL if incomplete */
```

## Memory model

- Paths are `const` and live in Flash; writers borrow caller storage.
- Measurement workspace is caller-owned (`pg_measure_sample_t array[...]`);
  `pg_path_buffer_t` borrows a caller-provided `pg_cmd_t` array.
- The runtime path (init once, query many times) performs **zero heap
  allocation**. A too-small workspace returns
  `PG_ERR_WORKSPACE_TOO_SMALL` instead of silently truncating geometry, and
  `pg_measure_init()` is fail-atomic (object zeroed on error).
- Sizing guide: 128 samples cover typical instrument paths at 0.5-unit
  tolerance; grow the array when init reports
  `PG_ERR_WORKSPACE_TOO_SMALL`. Recursion is capped by `PG_MAX_RECURSION`
  (default 12) regardless of tolerance.

## Coordinates and conventions

- Screen coordinates: `+x` right, `+y` down.
- `t` (curve parameter) is always clamped to `[0, 1]`.
- `normalized` queries clamp to `[0, 1]`: `0` -> path start, `1` -> path end.
- `pg_tangent_to_normal()` returns `(-t.y, t.x)`.
- Multi-subpath semantics: MOVEs only move the cursor, a MOVE to the current
  position is a no-op, and all subpaths contribute to one continuous arc
  distance (the jump has zero length). A distance landing exactly on a
  contour joint resolves to the later command with `t = 0`; slices emit an
  extra `move_to` at such jumps instead of drawing a connecting line.
- All public APIs define degenerate behavior (zero-length segments, repeated
  control points, collinear backtracking, empty/MOVE-only paths): no crash,
  no NaN/Inf, explicit `pg_result_t` codes. Collinear overshoot is
  subdivided by the shared flatness engine, so lengths such as
  `M(0,0) Q(100,0) (10,0)` are measured (~95.2632), not reduced to the chord.

## Floating point

v1 uses `float` with `f`-suffixed literals throughout. All geometric
predicates share `PG_EPSILON`. No fixed-point until benchmarks demand it.
Positioning accuracy is validated against dense-sampling oracles within
0.2% of arc length; the returned distance parameter is LUT-interpolated
(binary search + local t interpolation) and the position is evaluated on the
original curve, so it is on-curve but not claimed to be an exact
arc-length point.

## Limitations (v1)

- Single-precision only; no NURBS/B-spline/Catmull-Rom, no 3D.
- `pg_measure_init()` flattens whole commands; slicing exists, path boolean
  operations and offset curves are out of scope.
- Multi-subpath paths measure one continuous distance; the future gauge
  accepts a single open contour.
- Threading: objects are reentrant for concurrent read-only queries on
  separate objects; no internal synchronization. LVGL APIs must run on the
  LVGL thread.

## License

MIT, see [LICENSE](LICENSE). All code is original implementation; do not
introduce GPL/AGPL material (it would block future LVGL upstream work).
