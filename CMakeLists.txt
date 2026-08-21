cmake_minimum_required(VERSION 3.16)
project(hhms VERSION 0.0.3 LANGUAGES C)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()


set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HHMS_BUILD_GUI "Build the raylib companion window" ON)
option(HHMS_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(HHMS_MACOS_BUNDLE "Build hhms as a macOS application bundle" OFF)
include(GNUInstallDirs)

function(hhms_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

set(HHMS_SANITIZERS_SUPPORTED OFF)
if(NOT MSVC AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(HHMS_SANITIZERS_SUPPORTED ON)
endif()

if(HHMS_ENABLE_SANITIZERS AND NOT HHMS_SANITIZERS_SUPPORTED)
    message(WARNING "HHMS_ENABLE_SANITIZERS is not supported by ${CMAKE_C_COMPILER_ID}")
endif()

function(hhms_enable_sanitizers target)
    if(HHMS_ENABLE_SANITIZERS AND HHMS_SANITIZERS_SUPPORTED)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        get_target_property(target_type ${target} TYPE)
        if(NOT target_type STREQUAL "STATIC_LIBRARY")
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

add_library(hhms_core STATIC
    src/hhms.c
    src/solver.c
    src/persistence.c
)
target_include_directories(hhms_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(UNIX)
    target_link_libraries(hhms_core PUBLIC m)
endif()
hhms_enable_warnings(hhms_core)
hhms_enable_sanitizers(hhms_core)

add_library(hhms_app STATIC src/app.c)
target_include_directories(hhms_app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(hhms_app PUBLIC HHMS_VERSION="${PROJECT_VERSION}")
target_link_libraries(hhms_app PUBLIC hhms_core)
hhms_enable_warnings(hhms_app)
hhms_enable_sanitizers(hhms_app)

add_executable(hhms_test tests/test_hhms.c)
target_link_libraries(hhms_test PRIVATE hhms_core)
hhms_enable_warnings(hhms_test)
hhms_enable_sanitizers(hhms_test)

add_executable(hhms_app_test tests/test_app.c)
target_link_libraries(hhms_app_test PRIVATE hhms_app)
hhms_enable_warnings(hhms_app_test)
hhms_enable_sanitizers(hhms_app_test)

enable_testing()
add_test(NAME hhms_test COMMAND hhms_test)
add_test(NAME hhms_app_test COMMAND hhms_app_test)

if(HHMS_BUILD_GUI)
    enable_language(CXX)
    include(FetchContent)

    set(BUILD_EXAMPLES OFF CACHE BOOL "Build raylib examples" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "Build raylib games" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared dependencies" FORCE)
    set(NFD_BUILD_TESTS OFF CACHE BOOL "Build NFDe tests" FORCE)
    set(NFD_BUILD_SDL2_TESTS OFF CACHE BOOL "Build NFDe SDL2 tests" FORCE)
    set(NFD_INSTALL OFF CACHE BOOL "Install NFDe" FORCE)
    set(NFD_APPEND_EXTENSION ON CACHE BOOL "Append the selected file extension" FORCE)
    if(UNIX AND NOT APPLE)
        set(NFD_PORTAL OFF CACHE BOOL "Use NFDe's GTK backend" FORCE)
    endif()

    FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
        URL_HASH SHA256=aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e
    )
    FetchContent_Declare(
        nfd
        GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
        GIT_TAG fc168e8605bfa51aaec22ab0c4e46b9de665a437
    )
    FetchContent_GetProperties(raylib)
    if(NOT raylib_POPULATED)
        FetchContent_Populate(raylib)
        add_subdirectory("${raylib_SOURCE_DIR}" "${raylib_BINARY_DIR}" EXCLUDE_FROM_ALL)
    endif()
    FetchContent_MakeAvailable(nfd)

    set(HHMS_GUI_SOURCES
        src/main.c
        src/gui.c
        src/dialogs.c
    )
    if(APPLE AND HHMS_MACOS_BUNDLE)
        add_executable(hhms MACOSX_BUNDLE ${HHMS_GUI_SOURCES})
        set_target_properties(hhms PROPERTIES
            OUTPUT_NAME "HH Minesweeper"
            MACOSX_BUNDLE_BUNDLE_NAME "HH Minesweeper"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.hhminesweeperassistant.hhms"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        )
    else()
        add_executable(hhms ${HHMS_GUI_SOURCES})
    endif()

    target_link_libraries(hhms PRIVATE hhms_app raylib nfd::nfd)
    hhms_enable_warnings(hhms)
    hhms_enable_sanitizers(hhms)
    if(APPLE)
        target_compile_definitions(hhms PRIVATE GL_SILENCE_DEPRECATION)
    endif()

    install(
        TARGETS hhms
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        BUNDLE DESTINATION "."
    )
endif()

install(
    FILES README.md LICENSE CHANGELOG.md
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)
install(
    DIRECTORY docs/screenshots/
    DESTINATION "${CMAKE_INSTALL_DOCDIR}/docs/screenshots"
    FILES_MATCHING PATTERN "*.png"
)
install(
    DIRECTORY examples/
    DESTINATION "${CMAKE_INSTALL_DATADIR}/hhms/examples"
    FILES_MATCHING PATTERN "*.hhmap"
)
