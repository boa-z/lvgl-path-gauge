add_library(path2d STATIC
    src/pg_path.c
    src/pg_bezier.c
    src/pg_flatten.c
    src/pg_measure.c
)
target_include_directories(path2d PUBLIC include)
if(NOT MSVC)
    target_link_libraries(path2d PUBLIC m)
endif()
