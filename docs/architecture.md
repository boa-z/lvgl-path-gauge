# Architecture

## Repository dependency direction

    application
       |-- lvgl-path-gauge  (widget, rendering, examples)
       |       |-- LVGL    (object lifecycle and drawing)
       |       +-- lv-path (generic geometry only)
       +-- lv-path         (optional direct geometry consumer)

lv-path never depends on LVGL or lvgl-path-gauge. It accepts geometric
commands and caller storage, and emits positions, tangents or path commands.
It has no value ranges, colours, SOC or vehicle concepts. The widget maps
application values and zones onto the measured path and issues LVGL draws.
Product-specific paths and styling belong to examples or applications.

## Geometry ownership

All former libs/path2d headers and sources moved to the separate lv-path
repository at third_party/lv-path. Its source, tests, license and C99 build
are self-contained. Public path2d/pg_*.h headers and pg_* symbols are retained
to keep this extraction source-compatible. There is one engine target,
lv_path::lv_path (path2d::path2d is a compatibility alias).

The dependency is pinned by a Git submodule commit. CMake may reuse an
already supplied lv_path::lv_path target or explicitly selected source root;
it never downloads geometry at configure time. See [migration](migration.md)
for checkout and publication details.

Subdivision, flattening, arc-length approximation, slicing and numerical
contracts belong to lv-path; its src/pg_subdiv.c is the shared subdivision
implementation. The widget's single-open-contour restriction is a widget
policy and does not constrain the generic engine's multi-contour support.

## Gauge pipeline (Phase 4.5 + 5)

```text
lv_path_gauge_set_path()                    once per path
  topology check (exactly one MOVE, no CLOSE)
  pg_measure_init()      -> arc-length LUT in caller samples storage
  pg_path_flatten()      -> caller vertices + cumulative distances
  metadata (pg_measure_t, vertex count, total distance) -> private instance

lv_path_gauge_set_value()                   per frame
  clamp -> store -> lv_obj_invalidate()

LV_EVENT_DRAW_MAIN
  LV_PART_MAIN      : draw all cached segments               (track)
  LV_PART_INDICATOR : draw segments clipped to [0, d(value)] (progress)
    zone_count == 0 : one sub-run, indicator base colour
    zone_count  > 0 : partition [min, value] into base/zone sub-runs
```

Progress cutting is pure cache arithmetic: each segment is interpolated by its
stored distances, so no measurement, slicing or flattening happens per frame.
`LV_EVENT_REFR_EXT_DRAW_SIZE` enlarges the invalid area by half the widest
stroke plus any path point outside the object box, so thick strokes are never
clipped. `LV_EVENT_GET_SELF_SIZE` reports the path bounding box for
`LV_SIZE_CONTENT`. Everything uses public LVGL API only (drawing mirrors
LVGL's own `lv_line`).

### API freeze (Phase 4.5)

`lv_path_gauge_workspace_t` is a pointer+capacity descriptor over caller
storage; it contains no arrays, so its layout does not change with any
capacity macro and the public ABI is stable. All runtime metadata lives in
the private `lv_path_gauge_t`; the render-cache introspection getters are
gone and `lv_path_gauge_clear_path()` owns the clearing semantics
(`set_path(NULL)` is a caller error). Tolerance handling is uniform: NaN/Inf
is rejected everywhere, `<= 0` selects the default.

### Zone rendering (Phase 5)

Zones are half-open value ranges `[start, end)` with a colour (start included,
end excluded; a boundary value belongs to the later zone and yields zero
visible length there), validated (ascending, non-overlapping, `start < end`)
and copied atomically into the fixed `LV_PATH_GAUGE_MAX_ZONES` slot table
inside the instance — no per-zone objects, no geometry rebuild. Drawing
partitions the active window `[min_value, value]` into base-colour gaps and
zone-coloured sub-runs clipped to the current range; the stored zones are
never rewritten.

Stroke continuity (Phase 5.1): inside one continuous colour run every
polyline joint is filled with a same-colour round cap (LVGL renders the cap
as a disc of the line width), so butt-joint wedges can never leak the
background or the track. Only the true start and end of the whole stroke
follow `line_rounded`; zone boundaries and the progress end keep a flat butt
cut, so adjacent colours never overlap with caps.

## Ownership and lifetime

| Object | Owner | Borrowed by | Lifetime rule |
|---|---|---|---|
| `pg_path_t` | application (usually `const`/Flash) | measure, flatten, gauge | must outlive the derived objects |
| `pg_measure_sample_t[]` | application | `pg_measure_t` | must outlive queries |
| `pg_cmd_t[]` buffer | application | `pg_path_buffer_t` | must outlive the writer/to_path view |
| `lv_path_gauge_workspace_t` | application | gauge | pointer+capacity descriptor; the pointed-to arrays must outlive the gauge's use of the path |
| `lv_path_gauge_zone_t[]` copy | gauge instance | — | copied by `set_zones()` into fixed `LV_PATH_GAUGE_MAX_ZONES` slots |

## Measurement pipeline

```text
pg_measure_init
  validate path (structure + finite coordinates)
  walk commands (MOVE moves the cursor, CLOSE returns to the subpath start)
  shared subdivision -> leaf chords accumulate distance
  append (distance, t, command_index) into caller workspace
  total_length or an explicit error; failure zeroes the object (fail-atomic)

pg_measure_get_pos_tan[_normalized]
  clamp distance, binary search O(log N)
  interpolate t inside the bracket (command-aware at contour joints)
  evaluate the ORIGINAL curve at t (LUT only locates)
  tangent tiers: derivative -> adjacent measurable span -> chord -> (1, 0)

pg_measure_slice
  locate both ends; MOVE at the slice start
  per command in range: restrict the curve with two De Casteljau splits
  (degree preserved, never a polyline), straight pieces -> line_to
  every MOVE command in range -> additional move_to (topology, not geometry)
```

Cost per query: one endpoint walk over commands + `O(log N)` search + one
curve evaluation + one derivative. No per-frame flatten, no heap.

## Memory

```text
const path (Flash)  +  caller workspace (RAM, fixed)  +  zero runtime heap
```

Capacities are compile-time visible (`PG_MEASURE_MIN_SAMPLES`; the gauge's
`LV_PATH_GAUGE_MAX_SAMPLES` / `LV_PATH_GAUGE_MAX_VERTICES` are only
recommended array sizes). The workspace descriptor carries pointers and
capacities, so it stays 40 bytes on 64-bit hosts regardless of them. Writers
own no storage: `pg_path_buffer_t` borrows a caller-provided `pg_cmd_t` array;
zone tables are copied into the fixed `LV_PATH_GAUGE_MAX_ZONES` slots of the
widget instance.

## Upstream note

The candidate for upstream discussion is the independent lv-path geometry
repository. Public naming, integration and numerical/resource contracts
still need maintainer review; see [migration](migration.md). Gauge application
policies and demos remain in this repository. No upstream proposal, API
renaming or new widget feature is part of this split.
