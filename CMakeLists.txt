cmake_minimum_required(VERSION 3.16)
project(lvgl-path-gauge VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(PATH2D_SANITIZE "Enable AddressSanitizer+UndefinedBehaviorSanitizer (host only)" OFF)
option(PATH2D_BUILD_TESTS "Build host tests" ON)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
elseif(MSVC)
    add_compile_options(/W4 /WX)
endif()

if(PATH2D_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "PATH2D_SANITIZE is not supported with MSVC")
    endif()
    add_compile_options(-fsanitize=address,undefined -g)
    add_link_options(-fsanitize=address,undefined)
endif()

add_subdirectory(libs/path2d)

if(PATH2D_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
