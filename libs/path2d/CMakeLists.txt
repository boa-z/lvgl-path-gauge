add_library(path2d STATIC
    src/pg_path.c
    src/pg_bezier.c
    src/pg_subdiv.c
    src/pg_writer.c
    src/pg_slice.c
    src/pg_flatten.c
    src/pg_measure.c
)
target_include_directories(path2d PUBLIC include)
target_link_libraries(path2d PRIVATE path2d_dev_flags)
add_library(path2d::path2d ALIAS path2d)
if(NOT MSVC)
    target_link_libraries(path2d PUBLIC m)
endif()
