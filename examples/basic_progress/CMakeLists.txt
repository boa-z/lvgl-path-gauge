add_executable(basic_progress main.c)
target_link_libraries(basic_progress PRIVATE lv_path_gauge::lv_path_gauge)
target_link_libraries(basic_progress PRIVATE path2d_dev_flags)

# Optional SDL2 window front-end (PC viewing). SDL2 itself is attached to the
# LVGL target in src/CMakeLists.txt; here only the feature flag is set. The
# headless memory-display path stays the default so CI and machines without
# SDL2 keep working.
if(LV_PATH_GAUGE_SDL)
    target_compile_definitions(basic_progress PRIVATE LV_PATH_GAUGE_SDL=1)
endif()

if(PATH2D_BUILD_TESTS)
    add_test(NAME basic_progress_smoke
             COMMAND basic_progress --smoke --output ${CMAKE_CURRENT_BINARY_DIR})
endif()
