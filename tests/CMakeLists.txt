# path2d host tests (no LVGL required).
foreach(t bezier path flatten measure writer slice)
    add_executable(test_${t} test_${t}.c)
    target_link_libraries(test_${t} PRIVATE path2d::path2d path2d_dev_flags)
    target_include_directories(test_${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${t} COMMAND test_${t})
endforeach()

add_executable(bench_measure bench_measure.c)
target_link_libraries(bench_measure PRIVATE path2d::path2d path2d_dev_flags)
target_include_directories(bench_measure PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME bench COMMAND bench_measure)
set_tests_properties(bench PROPERTIES PASS_REGULAR_EXPRESSION "queries/sec")

# LVGL integration tests (only when the gauge widget is being built).
if(LV_PATH_GAUGE_BUILD)
    foreach(t lv_path_gauge lv_path_gauge_draw lv_path_gauge_zones)
        add_executable(test_${t} test_${t}.c)
        target_link_libraries(test_${t} PRIVATE lv_path_gauge::lv_path_gauge
                                                path2d_dev_flags)
        target_include_directories(test_${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME ${t} COMMAND test_${t})
    endforeach()
endif()
