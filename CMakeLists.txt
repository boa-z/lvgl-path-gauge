cmake_minimum_required(VERSION 3.16)
project(lvgl-path-gauge VERSION 0.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(PATH2D_SANITIZE "Enable AddressSanitizer+UndefinedBehaviorSanitizer (host only)" OFF)

# Tests default to ON for standalone builds and OFF when this repository is
# consumed as a subproject.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(PATH2D_TOP_LEVEL ON)
else()
    set(PATH2D_TOP_LEVEL OFF)
endif()
option(PATH2D_BUILD_TESTS "Build host tests" ${PATH2D_TOP_LEVEL})

# Target-scoped development flags: libraries/tests opt in explicitly, so
# neither directory scopes nor installed consumers inherit -Werror or
# sanitizer instrumentation.
add_library(path2d_dev_flags INTERFACE)
target_compile_options(path2d_dev_flags INTERFACE
    $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Werror>
    $<$<COMPILE_LANG_AND_ID:C,MSVC>:/W4;/WX>)
if(PATH2D_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "PATH2D_SANITIZE is not supported with MSVC")
    endif()
    target_compile_options(path2d_dev_flags INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(path2d_dev_flags INTERFACE -fsanitize=address,undefined)
endif()

add_subdirectory(libs/path2d)

if(PATH2D_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
