# LVGL 9.1 integration: the gauge widget (Phase 4).
#
# LVGL is used at the exact pinned version (v9.1.0) either from an existing
# source tree (LV_PATH_GAUGE_LVGL_ROOT, e.g. a vendor SDK copy or a local
# checkout) or fetched by CMake. Only public LVGL API is used.

if(LV_PATH_GAUGE_LVGL_ROOT)
    message(STATUS "lv_path_gauge: using LVGL source tree ${LV_PATH_GAUGE_LVGL_ROOT}")
    # This widget needs neither the LVGL examples/demos nor the ThorVG
    # backend; disabling them also keeps partial checkouts buildable.
    set(LV_CONF_BUILD_DISABLE_EXAMPLES ON)
    set(LV_CONF_BUILD_DISABLE_DEMOS ON)
    set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON)
    add_subdirectory(${LV_PATH_GAUGE_LVGL_ROOT} ${CMAKE_BINARY_DIR}/lvgl EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    message(STATUS "lv_path_gauge: fetching LVGL v9.1.0")
    FetchContent_Declare(lvgl
        GIT_REPOSITORY https://github.com/lvgl/lvgl.git
        GIT_TAG v9.1.0
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(lvgl)
endif()

if(NOT TARGET lvgl::lvgl)
    message(FATAL_ERROR "lv_path_gauge: LVGL target lvgl::lvgl not available")
endif()

# Optional SDL2 window driver (LV_USE_SDL is enabled by config/lv_conf_sdl.h).
# SDL2 is attached to the LVGL target because the driver sources are compiled
# there; the widget itself stays platform independent.
if(LV_PATH_GAUGE_SDL)
    find_package(SDL2 CONFIG QUIET)
    if(NOT SDL2_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL2_PC QUIET sdl2)
        endif()
    endif()

    if(TARGET SDL2::SDL2)
        target_link_libraries(lvgl PUBLIC SDL2::SDL2)
        set(LV_PATH_GAUGE_SDL_TARGET SDL2::SDL2)
    elseif(TARGET SDL2::SDL2-static)
        target_link_libraries(lvgl PUBLIC SDL2::SDL2-static)
        set(LV_PATH_GAUGE_SDL_TARGET SDL2::SDL2-static)
    elseif(SDL2_PC_FOUND)
        target_include_directories(lvgl SYSTEM PUBLIC ${SDL2_PC_INCLUDE_DIRS})
        target_link_libraries(lvgl PUBLIC ${SDL2_PC_LIBRARIES})
        target_link_directories(lvgl PUBLIC ${SDL2_PC_LIBRARY_DIRS})
        set(LV_PATH_GAUGE_SDL_TARGET "pkg-config sdl2")
    else()
        message(FATAL_ERROR
            "LV_PATH_GAUGE_SDL=ON but SDL2 was not found. Install the SDL2 "
            "development package (MSYS2 UCRT64: pacman -S "
            "mingw-w64-ucrt-x86_64-SDL2) or pass "
            "-DSDL2_DIR=<prefix>/lib/cmake/SDL2")
    endif()
    message(STATUS "lv_path_gauge: SDL2 window driver via ${LV_PATH_GAUGE_SDL_TARGET}")
endif()

add_library(lv_path_gauge STATIC lv_path_gauge.c)
target_include_directories(lv_path_gauge
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lv_path_gauge PUBLIC lvgl::lvgl path2d::path2d)
target_link_libraries(lv_path_gauge PRIVATE path2d_dev_flags)
add_library(lv_path_gauge::lv_path_gauge ALIAS lv_path_gauge)
