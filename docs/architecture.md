# Architecture

```text
SVG design稿
  |
  v                 (Phase 7, host-side only)
pg_path             static const commands, Flash resident
  |
  v                 (Phase 2)
pg_measure          adaptive flatten -> arc-length LUT (caller workspace)
  |                   binary search + exact curve re-evaluation
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
- `Geometry != Rendering`: `pg_measure` never calls draw APIs; it hands out
  points, tangents and slices, the renderer decides how to paint them.
- `Rendering != Vehicle logic`: the gauge owns `value` only.
- One path is the single geometric truth: track, progress, ticks and the
  needle all derive from the same `pg_path_t`. No parallel
  `needle_positions[]` / `tick_positions[]` tables.

## Measurement pipeline (Phase 2)

```text
pg_measure_init
  validate path
  walk commands (MOVE sets cursor, CLOSE returns to subpath start)
  adaptive subdivision per segment (flatness = control-to-chord distance)
  append (distance, t, command_index) into caller workspace
  total_length or PG_ERR_DEGENERATE / PG_ERR_WORKSPACE_TOO_SMALL

pg_measure_get_pos_tan[_normalized]
  clamp distance, binary search O(log N)
  interpolate t inside the bracket (command-aware at contour joints)
  evaluate the ORIGINAL curve at t (LUT only locates)
  tangent = unit curve derivative, chord then (1,0) fallback
```

Cost per query: one endpoint walk over commands + `O(log N)` search +
one curve evaluation + one derivative. No per-frame flatten, no heap.

## Memory

```text
const path (Flash)  +  caller workspace (RAM, fixed)  +  zero runtime heap
```

Capacities are compile-time visible (`PG_MEASURE_MIN_SAMPLES`,
per-gauge `LV_PATH_GAUGE_MAX_*` in later phases).

## Upstream note

The piece with upstream value is **path measurement**
(length / point-at-distance / tangent / slice), not the gauge widget.
A future `docs/upstream-plan.md` (Phase 10) will map `pg_measure_*` onto a
candidate LVGL vector-path measurement API. Product APIs must not be held
hostage to future LVGL assumptions.
