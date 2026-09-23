add_executable(segmented_soc main.c)
target_link_libraries(segmented_soc PRIVATE lv_path_gauge::lv_path_gauge)
target_link_libraries(segmented_soc PRIVATE path2d_dev_flags)

# Optional SDL2 window front-end (PC viewing). SDL2 itself is attached to the
# LVGL target in src/CMakeLists.txt; here only the feature flag is set. The
# headless memory-display path stays the default so CI and machines without
# SDL2 keep working.
if(LV_PATH_GAUGE_SDL)
    target_compile_definitions(segmented_soc PRIVATE LV_PATH_GAUGE_SDL=1)
endif()

if(PATH2D_BUILD_TESTS)
    add_test(NAME segmented_soc_smoke
             COMMAND segmented_soc --smoke --output ${CMAKE_CURRENT_BINARY_DIR})
endif()
