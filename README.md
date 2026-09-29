# lvgl-path-gauge

An LVGL component for rendering an arbitrary open path as a track and value
progress stroke, with optional fixed-capacity colour zones.

The generic geometry engine is maintained separately in **lv-path**. This
repository owns the widget, LVGL integration and demos; it has no private
copy of the geometry implementation.

    Application -> lv_path_gauge -> lv-path (C99 geometry)
                            |
                            +----> LVGL (rendering and object lifecycle)

See [architecture](docs/architecture.md), [migration](docs/migration.md) and
[split validation](docs/split-validation.md). Historical requirements and
task-book documents are retained for provenance, not as new-feature scope.

## Layout

| Path | Responsibility |
| --- | --- |
| include/lv_path_gauge.h | Public widget API and workspace descriptor |
| src/ | Widget implementation and LVGL build integration |
| third_party/lv-path/ | Pinned, independent geometry Git submodule |
| config/ | Host LVGL configurations |
| examples/basic_progress/ | Progress demo, memory display or SDL2 |
| examples/segmented_soc/ | Application-oriented zone demo, memory or SDL2 |
| tests/ | Widget lifecycle, drawing, zones and stroke-joint regression tests |

## Build and test

CMake 3.16+, a C11 compiler for the widget/LVGL, and C99 for lv-path.

    git submodule update --init --recursive
    cmake -S . -B build -DLV_PATH_GAUGE_LVGL_ROOT=/path/to/lvgl
    cmake --build build
    ctest --test-dir build --output-on-failure

Omit LV_PATH_GAUGE_LVGL_ROOT to fetch the existing pinned LVGL v9.1.0.
The widget is now enabled by default. Standalone builds include four widget
tests and both demo smoke tests. Tests/examples default off in subprojects.
LV_PATH_GAUGE_BUILD_TESTS, LV_PATH_GAUGE_BUILD_EXAMPLES,
LV_PATH_GAUGE_STRICT_WARNINGS and LV_PATH_GAUGE_SANITIZE control the widget.
Development flags remain private to project targets.

For geometry alone (no LVGL and no network), build its repository directly:

    cmake -S third_party/lv-path -B build-path
    cmake --build build-path
    ctest --test-dir build-path --output-on-failure

Consumers may provide an existing lv_path::lv_path target or set
LV_PATH_GAUGE_PATH_ROOT to an independent checkout. Existing lvgl::lvgl
targets are reused, so an embedding application does not build LVGL twice.
Non-CMake builds compile src/lv_path_gauge.c plus the independent engine's
seven C sources and supply both include directories and their LVGL settings.

### SDL2 examples

Install the platform SDL2 development package, then configure with
-DLV_PATH_GAUGE_SDL=ON (and -DSDL2_DIR=<prefix>/lib/cmake/SDL2 if needed).
Run either example with --window, optionally --frames 300. Headless defaults
use --smoke --output <existing-directory> and write PPM reference frames.
SDL2 belongs only to the example/LVGL display configuration.

## Quick start


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

## Memory and scope

The widget borrows caller-owned path commands and sample/vertex/distance
arrays. set_path prepares geometry; set_value clamps, stores and invalidates.
Draws use the cached polyline. Zones are copied into fixed instance storage.
LVGL object creation/destruction follows LVGL's allocation model; geometry
requires no heap. All LVGL calls belong to the LVGL execution context.

Existing behavior is preserved: one open contour, track/progress, flat zone
boundaries and existing cap/joint behavior. No new widgets or geometry
features are added by the split. Generic multi-contour geometry and numerical
contracts are documented in [lv-path](third_party/lv-path/README.md).

## License

MIT; copyright notices and LICENSE are retained in both repositories.
