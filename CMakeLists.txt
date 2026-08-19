cmake_minimum_required(VERSION 3.16)
project(hhms C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

add_library(hhms_core STATIC
    src/hhms.c
    src/solver.c
)
target_include_directories(hhms_core PUBLIC src)
if(NOT MSVC)
    target_compile_options(hhms_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(hhms_test tests/test_hhms.c)
target_link_libraries(hhms_test PRIVATE hhms_core)
if(NOT MSVC)
    target_compile_options(hhms_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME hhms_test COMMAND hhms_test)

option(HHMS_BUILD_GUI "Build the raylib companion window" ON)
if(HHMS_BUILD_GUI)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(raylib)

    add_executable(hhms src/main.c)
    target_link_libraries(hhms PRIVATE hhms_core raylib)
    if(NOT MSVC)
        target_compile_options(hhms PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(APPLE)
        target_compile_definitions(hhms PRIVATE GL_SILENCE_DEPRECATION)
    endif()
endif()
