add_executable(basic_progress main.c)
target_link_libraries(basic_progress PRIVATE lv_path_gauge::lv_path_gauge)
target_link_libraries(basic_progress PRIVATE path2d_dev_flags)

if(PATH2D_BUILD_TESTS)
    add_test(NAME basic_progress_smoke
             COMMAND basic_progress --smoke --output ${CMAKE_CURRENT_BINARY_DIR})
endif()
