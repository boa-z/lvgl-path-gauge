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
lv_path_gauge        LVGL 9.1.0 widget (Phase 4)
      |
path2d               pure C11, no LVGL/RTOS/heap
```

Current status: **Phase 0–5 complete** — `path2d` core (LINE/QUAD/CUBIC,
evaluate/derivative/split, shared adaptive flatten, arc-length LUT,
`pg_measure_get_pos_tan[_normalized]()`), geometry hardening, writers/slicing,
the **LVGL gauge** (single open contour: static track plus progress) with the
frozen production API (Phase 4.5) and **fixed-capacity value-domain zones**
(Phase 5). Ticks, needles, labels, animations, the SVG tool and the vector
renderer are still to come.

Normative documents: [original requirements](docs/requirements.md) §1–107 and
[task book amendments](docs/task-book.md) §108–117.

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
include/lv_path_gauge.h      gauge widget API + caller-owned workspace
src/                         widget implementation + LVGL integration CMake
config/lv_conf.h             host/CI LVGL 9.1 configuration (reference)
examples/basic_progress/     0 -> 100 -> 0 demo (memory display + PPM output)
examples/segmented_soc/      three-zone SOC demo (zones, PPM/SDL window)
tests/                       host tests (CTest), benchmark and LVGL harness
docs/                        requirements, task book, architecture
```

## Build and test (host)

Requires CMake 3.16+ and a C11 compiler (GCC or Clang).

```bash
# path2d only: no LVGL, no network
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

# with the LVGL 9.1 gauge (fetches the pinned LVGL v9.1.0)
cmake -S . -B build-gauge -DLV_PATH_GAUGE_BUILD=ON
cmake --build build-gauge
ctest --test-dir build-gauge --output-on-failure
```

Offline / vendor SDK builds can point at an existing LVGL source tree:

```bash
cmake -S . -B build-gauge -DLV_PATH_GAUGE_BUILD=ON \
      -DLV_PATH_GAUGE_LVGL_ROOT=/path/to/lvgl
```

### Watching the animation on PC (SDL2 window)

The examples have an optional SDL2 window front-end so the animation can be
viewed interactively; the widget code is identical to the headless build.

```bash
# MSYS2 UCRT64: pacman -S mingw-w64-ucrt-x86_64-SDL2
cmake -S . -B build-sdl -DLV_PATH_GAUGE_BUILD=ON -DLV_PATH_GAUGE_SDL=ON \
      -DLV_PATH_GAUGE_LVGL_ROOT=/path/to/lvgl
cmake --build build-sdl
./build-sdl/examples/basic_progress/basic_progress --window     # 0 -> 100 -> 0
./build-sdl/examples/basic_progress/basic_progress --window --frames 300
./build-sdl/examples/segmented_soc/segmented_soc --window       # three-zone SOC
./build-sdl/examples/segmented_soc/segmented_soc --window --frames 300
```

`LV_PATH_GAUGE_SDL=ON` switches the LVGL configuration to
`config/lv_conf_sdl.h` (`LV_USE_SDL 1`, software rendering only) and attaches
SDL2 to the LVGL target; the gauge library itself never depends on SDL. The
headless path stays the default, and CI compiles/runs the windowed example
with `SDL_VIDEODRIVER=dummy` so the interactive path is still exercised.

Strict warnings are target-scoped (`path2d_dev_flags`): `-Wall -Wextra
-Wpedantic -Werror` for this project's targets only (`PATH2D_STRICT_WARNINGS`,
default ON standalone / OFF as a subproject), so consumers never inherit them.
Sanitizer run (host only):

```bash
cmake -S . -B build-asan -DPATH2D_SANITIZE=ON
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

`PATH2D_BUILD_TESTS` and `LV_PATH_GAUGE_BUILD_EXAMPLES` default to ON for
standalone builds and OFF as a subproject. CI (`.github/workflows/ci.yml`)
runs GCC + Clang, plain and `address,undefined` sanitizers, for path2d alone
and for the LVGL gauge against the pinned LVGL v9.1.0.

## Quick start (geometry only)

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
    /* writer overflow: the buffer is poisoned, buffer.failure holds the code */
}
pg_path_buffer_to_path(&buffer, &slice);
```

## Quick start (LVGL gauge)

```c
#include "lv_path_gauge.h"

static const pg_cmd_t soc_cmds[] = {           /* one open contour */
    PG_MOVE_TO(60.0f, 400.0f),
    PG_CUBIC_TO(60.0f, 280.0f, 200.0f, 320.0f, 320.0f, 240.0f),
};
static const pg_path_t soc_path = { soc_cmds, PG_ARRAY_SIZE(soc_cmds) };

static pg_measure_sample_t lut[LV_PATH_GAUGE_MAX_SAMPLES];
static pg_point_t verts[LV_PATH_GAUGE_MAX_VERTICES];
static float dists[LV_PATH_GAUGE_MAX_VERTICES];
static lv_path_gauge_workspace_t gauge_ws;     /* descriptor, not storage */

lv_path_gauge_workspace_init(&gauge_ws, lut, LV_PATH_GAUGE_MAX_SAMPLES,
                             verts, dists, LV_PATH_GAUGE_MAX_VERTICES, 0.5f);

lv_obj_t *gauge = lv_path_gauge_create(lv_screen_active());
lv_obj_set_size(gauge, 800, 480);              /* path coords are local px */
lv_path_gauge_set_path(gauge, &soc_path, &gauge_ws);
lv_path_gauge_set_range(gauge, 0, 100);
lv_path_gauge_set_value(gauge, 50);            /* clamp + invalidate only */
```

Styles: `LV_PART_MAIN` is the track, `LV_PART_INDICATOR` the active progress
(`line_width`, `line_color`, `line_opa`, `line_rounded`). Geometry joints
inside one colour run are always filled with same-colour caps, so thick
strokes show no background seams; `line_rounded` only controls the true
start/end of the stroke and zone boundaries stay flat. The path, the storage
arrays and the object must outlive each other as documented in the header;
`lv_path_gauge_clear_path(gauge)` clears the gauge explicitly.

Segmented SOC colours are value-domain zones (half-open `[start, end)`,
ascending, no overlap; gaps fall back to the indicator base colour):

```c
lv_path_gauge_zone_t zones[3];
zones[0] = (lv_path_gauge_zone_t){ 0, 20, lv_color_hex(0xFC0101) };
zones[1] = (lv_path_gauge_zone_t){ 20, 40, lv_color_hex(0xED6C00) };
zones[2] = (lv_path_gauge_zone_t){ 40, 100, lv_color_hex(0x0DD462) };
lv_path_gauge_set_zones(gauge, zones, 3);      /* atomic; no per-zone objects */
```

Zones only override the progress colour; `lv_path_gauge_clear_zones()` returns
to the single-colour progress. See `examples/segmented_soc`.

## Memory model

- Paths are `const` and live in Flash; the gauge borrows caller storage
  through `lv_path_gauge_workspace_t` (a pointer+capacity descriptor over the
  LUT, vertex and distance arrays). All runtime metadata lives in the private
  widget instance.
- The runtime path (init once, query/draw many times) performs **zero heap
  allocation**. `pg_measure_init()` is fail-atomic; a too-small workspace
  returns `PG_ERR_WORKSPACE_TOO_SMALL` instead of truncating geometry, and a
  failed `pg_path_buffer_t` write poisons the buffer (its original failure
  code is reported by `pg_path_buffer_to_path()`).
- The gauge is the only place that measures and flattens, and only inside
  `lv_path_gauge_set_path()`. `lv_path_gauge_set_value()` just clamps, stores
  and invalidates: progress is cut out of the cached polyline per frame.
- Zones are copied into the instance (fixed `LV_PATH_GAUGE_MAX_ZONES` slots,
  default 8) by `lv_path_gauge_set_zones()`; drawing adds no objects and no
  geometry work.
- Sizing guide: 128 samples cover typical instrument paths at 0.5-unit
  tolerance; grow the arrays when an init reports
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
  contour joint resolves to the later command with `t = 0`; slices and
  flattened output emit `move_to` at such contours instead of drawing a
  connecting line (boundaries come from MOVE commands, never from comparing
  coordinates).
- Degenerate inputs are defined, never NaN/Inf: zero-length spans, repeated
  control points, collinear backtracking, empty/MOVE-only paths. Collinear
  overshoot such as `M(0,0) Q(100,0) (10,0)` is measured (~95.2632), not
  reduced to its chord.
- Tangents use a four-tier fallback: analytic derivative, then the local
  direction of the adjacent measurable span, then the command chord, then
  `(1, 0)`. Collapsed control handles at either end therefore still report
  the true travel direction.

## Floating point

v1 uses `float` with `f`-suffixed literals throughout. All geometric
predicates share `PG_EPSILON`. No fixed-point until benchmarks demand it.
The returned distance parameter is LUT-interpolated (binary search + local t
interpolation) and the position is evaluated on the original curve, so it is
on-curve but not claimed to be an exact arc-length point; arc lengths are
validated against dense-sampling oracles within 0.2%.

## Limitations (v1)

- Single-precision only; no NURBS/B-spline/Catmull-Rom, no 3D.
- The gauge draws a static track and a value-progress stroke with optional
  fixed-capacity zones for one open contour: no ticks, needle, labels,
  animations, scaling/fitting or vector renderer yet (see the task book for
  the phase plan).
- `pg_measure_init()` measures whole commands; path boolean operations and
  offset curves are out of scope.
- Threading: objects are reentrant for concurrent read-only queries on
  separate objects; no internal synchronization. LVGL APIs must run on the
  LVGL thread.

## License

MIT, see [LICENSE](LICENSE). All code is original implementation; do not
introduce GPL/AGPL material (it would block future LVGL upstream work).
