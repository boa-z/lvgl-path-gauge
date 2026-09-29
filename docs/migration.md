# Split into lv-path and lvgl-path-gauge

## What moves

| Previous path | New repository and path |
| --- | --- |
| libs/path2d/include/path2d/pg_*.h (six) | lv-path: include/path2d/ |
| libs/path2d/src/pg_*.c (seven), pg_internal.h | lv-path: src/ |
| tests/test_{bezier,path,flatten,measure,writer,slice}.c | lv-path: tests/ |
| tests/bench_measure.c, test_recorder.h | lv-path: tests/ |
| tests/test_util.h | Preserved in both independent test suites |
| libs/path2d/CMakeLists.txt | Replaced by standalone lv-path CMakeLists.txt |

The MIT license and formatting metadata accompany the extracted history.
Widget sources/headers and both demo C files are unchanged. Public pg_* and
lv_path_gauge_* APIs, caller storage contracts and geometry behavior remain
unchanged. No mass symbol rename is part of this refactor.

## Build changes

- Clone recursively, or initialize third_party/lv-path explicitly.
- Geometry-only users build lv-path directly, link lv_path::lv_path and use
  LV_PATH_BUILD_TESTS / LV_PATH_STRICT_WARNINGS / LV_PATH_SANITIZE.
- Widget users keep lv_path_gauge::lv_path_gauge. Includes and the geometry
  target are propagated transitively. Use LV_PATH_GAUGE_* build options.
- The old path2d::path2d target remains an alias. Legacy PATH2D_* options in
  the gauge configure widget settings with a deprecation notice; they no
  longer select the relocated geometry test suite.
- The gauge defaults to building the widget. LV_PATH_GAUGE_BUILD=OFF remains
  accepted, but a standalone lv-path build is the geometry-only entry point.
- Manual/SCons source lists must replace libs/path2d/src and its include path
  with third_party/lv-path/src and third_party/lv-path/include. Compile the
  geometry sources once. The current D50T application only records this
  dependency; its tracked CMake/SCons targets do not yet compile the gauge.
- Existing application LVGL and lv-path CMake targets are reused. An external
  checkout can be selected with LV_PATH_GAUGE_PATH_ROOT.

## History and publication

Source baseline: f071402ae705e0f871a49002f25d0418432a7f73.
The original gauge history is not rewritten. Path-filtered Git fast-export /
fast-import creates a separate repository retaining relevant source/test
history, original authors, timestamps and messages. Paths are moved to the
new root, so commit hashes change. Mixed historical messages may mention
widget work, but those unrelated files are not included in the new history.

| Original geometry revision | Extracted revision |
| --- | --- |
| 6836e0a | 2a5e2de |
| 03ea1db | 67e796f |
| e6e6b58 | c34a0c0 |
| 012ef81 | 7d02a16 |

The repositories are hosted under boa-z:

- lv-path: https://github.com/boa-z/lv-path
- lvgl-path-gauge: https://github.com/boa-z/lvgl-path-gauge

The geometry pin is 927020e19925910d517687dc0823e9790fe83c28 and its history
is published first. The gauge references it through the relative submodule
URL ../lv-path.git. Use a recursive clone or initialize submodules after
checkout; no local URL override is required. The original gauge repository
is updated by a normal forward commit, without rewriting its history.

For later updates, publish lv-path first, then the gauge commit pinning it.
Refresh the consuming application's gauge gitlink and dependency-lock
commit together. Application and SDK integration commits remain separate
from these two public component repositories.

## Before upstream discussion

Agree naming/API ownership, supported scalar and coordinate contracts,
worst-case stack/workspace/time budgets, tolerance guarantees and behavior
at numerical extremes. Review licenses/provenance, fuzz/sanitizer coverage,
platform/compiler coverage and upstream integration conventions. The split
is a maintainability change, not an upstream-ready or hardware-pass claim.
