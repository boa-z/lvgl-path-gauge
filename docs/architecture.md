# Architecture

```text
SVG design稿
  |
  v                 (Phase 7, host-side only)
pg_path             static const commands, Flash resident
  |
  v                 (Phase 2)
pg_measure          shared adaptive subdivision -> arc-length LUT
  |                   (caller workspace) binary search + curve re-evaluation
  +--> pg_measure_slice -> pg_path_writer_t  (Phase 3)
  v                 (Phase 4+)
renderer            LVGL line backend: track / progress / zones / ticks
  |                   (vector backend stub reserved)
  v
lv_path_gauge       value/range/zones/needle widget, LVGL 9.1.0
  |
  v
application         CAN/filtering/vehicle logic lives here, never below
```

## Invariants

- `Path != Gauge`: `path2d` knows nothing of SOC, speed, ticks or needles.
- `Geometry != Rendering`: measurement and slicing never call draw APIs; they
  hand out points, tangents and commands, the renderer decides how to paint.
- `Rendering != Vehicle logic`: the gauge owns `value` only.
- One path is the single geometric truth: track, progress, ticks and the
  needle all derive from the same `pg_path_t`. No parallel
  `needle_positions[]` / `tick_positions[]` tables.

## Shared subdivision engine (Phase 2.5)

`pg_path_walk()` in `src/pg_subdiv.c` is the only place that subdivides
curves; `pg_path_flatten()` and `pg_measure_init()` are thin consumers.

A span is emitted when both flatness conditions hold:

```text
perpendicular deviation: max control distance to the chord <= tolerance
control-polygon excess:   polygon length - chord length   <= tolerance
```

The second condition exists because the perpendicular test alone is blind to
collinear overshoot/backtracking: for M(0,0) Q(100,0) (10,0) every control
point lies on the chord, yet the curve overshoots to x ~ 52.63 and returns to
x = 10 (arc length ~ 95.2632, chord 10). Monotone collinear spans have zero
excess and still flatten in a single step, so smooth instrument curves keep
their previous sample counts.

Bisection uses De Casteljau (`pg_quad_split` / `pg_cubic_split`) and is
bounded by `PG_MAX_RECURSION`; leaf spans carry the owning command index and
their local [t0, t1] so the LUT can use them directly.

## Gauge pipeline (Phase 4)

```text
lv_path_gauge_set_path()                    once per path
  topology check (exactly one MOVE, no CLOSE)
  pg_measure_init()      -> arc-length LUT (workspace)
  pg_path_flatten()      -> cached polyline + per-vertex cumulative distance
                            (workspace, caller-owned)

lv_path_gauge_set_value()                   per frame
  clamp -> store -> lv_obj_invalidate()

LV_EVENT_DRAW_MAIN
  LV_PART_MAIN      : draw all cached segments              (track)
  LV_PART_INDICATOR : draw segments clipped to [0, d(value)] (progress)
```

Progress cutting is pure cache arithmetic: each segment is interpolated by its
stored distances, so no measurement, slicing or flattening happens per frame.
`LV_EVENT_REFR_EXT_DRAW_SIZE` enlarges the invalid area by half the widest
stroke plus any path point outside the object box, so thick strokes are never
clipped. `LV_EVENT_GET_SELF_SIZE` reports the path bounding box for
`LV_SIZE_CONTENT`. Everything uses public LVGL API only (drawing mirrors
LVGL's own `lv_line`).

## Ownership and lifetime

| Object | Owner | Borrowed by | Lifetime rule |
|---|---|---|---|
| `pg_path_t` | application (usually `const`/Flash) | measure, flatten, gauge | must outlive the derived objects |
| `pg_measure_sample_t[]` | application | `pg_measure_t` | must outlive queries |
| `pg_cmd_t[]` buffer | application | `pg_path_buffer_t` | must outlive the writer/to_path view |
| `lv_path_gauge_workspace_t` | application | gauge | must outlive the gauge's use of the path |

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

Capacities are compile-time visible (`PG_MEASURE_MIN_SAMPLES`,
per-gauge `LV_PATH_GAUGE_MAX_*` in later phases). Writers own no storage:
`pg_path_buffer_t` borrows a caller-provided `pg_cmd_t` array.

## Upstream note

The piece with upstream value is **path measurement**
(length / point-at-distance / tangent / slice), not the gauge widget.
`docs/upstream-plan.md` (Phase 10) will map `pg_measure_*` onto a candidate
LVGL vector-path measurement API. Product APIs must not be held hostage to
future LVGL assumptions.
