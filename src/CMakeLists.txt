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

add_library(lv_path_gauge STATIC lv_path_gauge.c)
target_include_directories(lv_path_gauge
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lv_path_gauge PUBLIC lvgl::lvgl path2d::path2d)
target_link_libraries(lv_path_gauge PRIVATE path2d_dev_flags)
add_library(lv_path_gauge::lv_path_gauge ALIAS lv_path_gauge)
