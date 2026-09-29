# Widget tests only; geometry tests live in lv-path.
foreach(t lv_path_gauge lv_path_gauge_draw lv_path_gauge_zones lv_path_gauge_joints)
    add_executable(test_${t} test_${t}.c)
    target_link_libraries(test_${t} PRIVATE lv_path_gauge::lv_path_gauge lv_path_gauge_dev_flags)
    target_include_directories(test_${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${t} COMMAND test_${t})
endforeach()
