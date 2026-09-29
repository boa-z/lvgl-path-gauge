# Repository split validation — 2026-09-29

## Scope and provenance

- Original gauge baseline: f071402ae705e0f871a49002f25d0418432a7f73.
- Extracted lv-path pin: 927020e19925910d517687dc0823e9790fe83c28.
- Host: Windows, MinGW GCC 16.1.0, Ninja, CMake; ISO C99 for all standalone
  geometry targets, C11 for widget/LVGL.
- LVGL: existing SDK vendor 9.1 source at
  packages/artinchip/lvgl-ui/lvgl_v9/lvgl. Dependency-lock verification passed
  for all 594 locked vendor files.
- Widget/public header/demo C files unchanged from baseline. The seven
  geometry C sources and private header are unchanged; only two public
  coordinate comments were generalized. No rendering/geometry features added.

## Results

| Check | Result |
| --- | --- |
| Original vendor build, geometry + widget tests | PASS, 11/11 |
| Original basic_progress and segmented_soc smoke runs | PASS, run explicitly |
| Standalone lv-path C99 build and tests | PASS, 7/7 including benchmark |
| Independent Git clone of lv-path, no parent/renderer dependency | PASS, build and 7/7 |
| Separated widget and demo CTest suite | PASS, 6/6 |
| Eight demo frames compared with original baseline | PASS, byte-identical |
| SDL2 dummy-driver example loop, both demos | PASS, 20 frames each |
| SDL-configured CTest suite | PASS, 6/6 |
| Embedding with pre-existing lv-path and LVGL targets | PASS, 161 widget checks; no duplicate dependencies or test targets |
| Consumer compile flags | PASS, private -Werror does not propagate |
| Geometry archive undefined-symbol inspection | No allocator or LVGL symbols |
| Application dependency verifier | PASS, separate geometry pin and file hashes |
| Full firmware/image and physical D50T-2-Lite | NOT_RUN |
| Remote recursive checkout / hosted CI | Pending publication verification; initial extraction was tested locally |
| Linux GCC/Clang ASan/UBSan matrices | Run by the published GitHub Actions workflows; local host results above are separate |

The original CMake enabled testing after descending into examples, so those
smoke registrations were not discovered. The split enables testing first,
and both existing smoke programs now participate in CTest. Baseline examples
were run directly to avoid treating undiscovered tests as passes.

The initial vendor build lacked CACHE_LINE_SIZE and failed in vendor LVGL.
Both before/after builds use the same host compiler definition
-DCACHE_LINE_SIZE=32; no SDK/vendor file was modified. Attempts to fetch
upstream LVGL v9.1.0 failed due to transport/certificate-revocation errors;
these results cover the locally locked vendor source, not a freshly fetched
upstream tree. SDL uses the installed UCRT64 SDL2 package and dummy video
driver, not a physical display. No board, CAN, serial or flashing operation
was performed. The application currently records the component dependency
but does not compile it in its tracked CMake/SCons targets; no firmware runtime
source or configuration changed.

## Commands and local evidence

From the gauge repository (set SDK_ROOT to the existing SDK checkout):

    cmake -S third_party/lv-path -B third_party/lv-path/build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    cmake --build third_party/lv-path/build
    ctest --test-dir third_party/lv-path/build --output-on-failure

    cmake -S . -B build-after-split -G Ninja -DLV_PATH_GAUGE_LVGL_ROOT=<SDK_ROOT>/packages/artinchip/lvgl-ui/lvgl_v9/lvgl -DCMAKE_C_FLAGS=-DCACHE_LINE_SIZE=32
    cmake --build build-after-split
    ctest --test-dir build-after-split --output-on-failure

SDL additionally uses -DLV_PATH_GAUGE_SDL=ON and
-DSDL2_DIR=C:/msys64/ucrt64/lib/cmake/SDL2 in build-sdl-split. Put the SDL2 DLL
folder on PATH and set SDL_VIDEODRIVER=dummy, then run each example with
--window --frames 20. The independent clone is build-independent and its
CTest directory is build-independent/build. Build outputs are ignored.

Logs: build-before-split/build.log, build-after-split/build.log,
build-sdl-split/build.log, and each build's Testing/Temporary/LastTest.log.
The original and post-split PPM files remain under their respective
build-before-split/examples and build-after-split/examples directories.
Application verification command: python -X utf8 tools/quality/verify_dependencies.py.

## Frame hashes (identical before and after)

| Relative frame path | SHA256 |
| --- | --- |
| basic_progress/basic_progress_000.ppm | 0155236bbcdd4e62efff88f4d65ac8328ef0f8fbb97a22c2ac4e39e2f664d595 |
| basic_progress/basic_progress_050.ppm | 533d471d64896841f260ac0db6001f3ea79ce5cf6cc35d4f68071bdcb9456b8b |
| basic_progress/basic_progress_100.ppm | b134a596876abc20c9ac7aa50229c1c5d79284ddb0f3270c0357720f56e5862e |
| segmented_soc/segmented_soc_000.ppm | 329bff5872b8632503eeddbf932863e13b0235f0f0cfd6b8d39ed7d27db83b31 |
| segmented_soc/segmented_soc_020.ppm | 978d02cc27c0545d449492076da24019c0e245367333f53b095af6cac250ac1a |
| segmented_soc/segmented_soc_040.ppm | 403f20097861f274ce262b0686504dbf90599ba2851970c3107027488e6b8861 |
| segmented_soc/segmented_soc_053.ppm | 0bbfd29f113c6052d67fba2b5ad6dd06a7ccc9342a390bb4b51a442949d80253 |
| segmented_soc/segmented_soc_100.ppm | aacab87e7936fa34a0909824946907bd268a3d04a6e11b0b28433407007280fd |

## Publication

The geometry repository is hosted at https://github.com/boa-z/lv-path and
the widget repository remains https://github.com/boa-z/lvgl-path-gauge.
Geometry history and its pinned commit are pushed before the gauge split.
The sibling submodule URL ../lv-path.git now resolves to the hosted geometry
repository; use recursive checkout.

The new standalone-build and repository-split commits use the requested
author and committer identity boa-z <boa-z@outlook.com>. Imported historical
commits retain their original attribution. Application lock and SDK gitlink
updates are separate from these component repositories; no hardware
validation is implied by publishing.
