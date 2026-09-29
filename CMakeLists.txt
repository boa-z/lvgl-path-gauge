cmake_minimum_required(VERSION 3.16)
project(lvgl-path-gauge VERSION 0.5.0 LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(LV_PATH_GAUGE_TOP_LEVEL ON)
else()
    set(LV_PATH_GAUGE_TOP_LEVEL OFF)
endif()
# Preserve old caller options; geometry itself now uses LV_PATH_*.
foreach(suffix BUILD_TESTS STRICT_WARNINGS SANITIZE)
    if(DEFINED PATH2D_${suffix} AND NOT DEFINED LV_PATH_GAUGE_${suffix})
        set(LV_PATH_GAUGE_${suffix} "${PATH2D_${suffix}}")
        message(DEPRECATION "Use LV_PATH_GAUGE_${suffix} for widget settings or LV_PATH_${suffix} for geometry")
    endif()
endforeach()
option(LV_PATH_GAUGE_BUILD "Build the LVGL gauge widget" ON)
option(LV_PATH_GAUGE_BUILD_TESTS "Build widget tests" ${LV_PATH_GAUGE_TOP_LEVEL})
option(LV_PATH_GAUGE_BUILD_EXAMPLES "Build gauge examples" ${LV_PATH_GAUGE_TOP_LEVEL})
option(LV_PATH_GAUGE_STRICT_WARNINGS "Treat widget warnings as errors" ${LV_PATH_GAUGE_TOP_LEVEL})
option(LV_PATH_GAUGE_SANITIZE "Enable host address and undefined behavior sanitizers" OFF)

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

add_library(lv_path_gauge_dev_flags INTERFACE)
if(LV_PATH_GAUGE_STRICT_WARNINGS)
    target_compile_options(lv_path_gauge_dev_flags INTERFACE
        $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Werror>
        $<$<COMPILE_LANG_AND_ID:C,MSVC>:/W4;/WX>)
else()
    target_compile_options(lv_path_gauge_dev_flags INTERFACE
        $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
        $<$<COMPILE_LANG_AND_ID:C,MSVC>:/W4>)
endif()
if(LV_PATH_GAUGE_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "LV_PATH_GAUGE_SANITIZE is not supported with MSVC")
    endif()
    target_compile_options(lv_path_gauge_dev_flags INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(lv_path_gauge_dev_flags INTERFACE -fsanitize=address,undefined)
endif()

# Reuse an application-supplied target, otherwise consume the pinned checkout.
set(LV_PATH_GAUGE_PATH_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lv-path"
    CACHE PATH "Standalone lv-path source tree")
if(NOT TARGET lv_path::lv_path)
    if(NOT EXISTS "${LV_PATH_GAUGE_PATH_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR "lv-path is missing. Run git submodule update --init --recursive, or set LV_PATH_GAUGE_PATH_ROOT.")
    endif()
    if(NOT DEFINED LV_PATH_SANITIZE)
        set(LV_PATH_SANITIZE ${LV_PATH_GAUGE_SANITIZE})
    endif()
    if(NOT DEFINED LV_PATH_STRICT_WARNINGS)
        set(LV_PATH_STRICT_WARNINGS ${LV_PATH_GAUGE_STRICT_WARNINGS})
    endif()
    add_subdirectory("${LV_PATH_GAUGE_PATH_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/lv-path")
endif()
# Enable before examples so their smoke tests are registered.
if(LV_PATH_GAUGE_BUILD_TESTS)
    enable_testing()
endif()
if(LV_PATH_GAUGE_BUILD)
    add_subdirectory(src)
    if(LV_PATH_GAUGE_BUILD_EXAMPLES)
        add_subdirectory(examples/basic_progress)
        add_subdirectory(examples/segmented_soc)
    endif()
    if(LV_PATH_GAUGE_BUILD_TESTS)
        add_subdirectory(tests)
    endif()
endif()
