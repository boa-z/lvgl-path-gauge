cmake_minimum_required(VERSION 3.16)
project(lvgl-path-gauge VERSION 0.5.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(PATH2D_SANITIZE "Enable AddressSanitizer+UndefinedBehaviorSanitizer (host only)" OFF)

# Tests and strict warnings default to ON for standalone builds and OFF when
# this repository is consumed as a subproject.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(PATH2D_TOP_LEVEL ON)
else()
    set(PATH2D_TOP_LEVEL OFF)
endif()
option(PATH2D_BUILD_TESTS "Build host tests" ${PATH2D_TOP_LEVEL})
option(PATH2D_STRICT_WARNINGS "Treat project warnings as errors" ${PATH2D_TOP_LEVEL})

# --- Phase 4: LVGL arbitrary-path gauge -------------------------------------
# Off by default so the pure-C11 path2d library keeps building with no LVGL
# and no network access. CI and the README enable it explicitly.
option(LV_PATH_GAUGE_BUILD "Build the LVGL 9.1 path gauge widget, tests and example" OFF)
option(LV_PATH_GAUGE_BUILD_EXAMPLES "Build the gauge example programs" ${PATH2D_TOP_LEVEL})
set(LV_PATH_GAUGE_LVGL_ROOT "" CACHE PATH
    "Existing LVGL source tree to use instead of fetching LVGL v9.1.0")
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/config/lv_conf.h" CACHE STRING
    "lv_conf.h used by the gauge host builds")

# Optional SDL2 window front-end for the example programs (PC viewing).
# Switches the LVGL configuration to config/lv_conf_sdl.h; the widget itself
# never depends on SDL.
option(LV_PATH_GAUGE_SDL "Build the examples with the SDL2 window driver" OFF)
if(LV_PATH_GAUGE_SDL)
    set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/config/lv_conf_sdl.h" CACHE STRING
        "lv_conf.h used by the gauge host builds" FORCE)
endif()

# Target-scoped development flags: libraries/tests opt in explicitly, so
# neither directory scopes nor installed consumers inherit -Werror or
# sanitizer instrumentation.
add_library(path2d_dev_flags INTERFACE)
if(PATH2D_STRICT_WARNINGS)
    target_compile_options(path2d_dev_flags INTERFACE
        $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Werror>
        $<$<COMPILE_LANG_AND_ID:C,MSVC>:/W4;/WX>)
else()
    target_compile_options(path2d_dev_flags INTERFACE
        $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
        $<$<COMPILE_LANG_AND_ID:C,MSVC>:/W4>)
endif()
if(PATH2D_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "PATH2D_SANITIZE is not supported with MSVC")
    endif()
    target_compile_options(path2d_dev_flags INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(path2d_dev_flags INTERFACE -fsanitize=address,undefined)
endif()

add_subdirectory(libs/path2d)

if(LV_PATH_GAUGE_BUILD)
    add_subdirectory(src)
    if(LV_PATH_GAUGE_BUILD_EXAMPLES)
        add_subdirectory(examples/basic_progress)
        add_subdirectory(examples/segmented_soc)
    endif()
endif()

if(PATH2D_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
