cmake_minimum_required(VERSION 3.24)

project(win_butler VERSION 0.1.0 LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "win-butler currently targets Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "win-butler currently targets x86 (32-bit). Configure with -A Win32.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(WINBUTLER_BUILD_FIXTURES "Build local test fixtures" OFF)
option(WINBUTLER_BUILD_UI "Build the EUI-NEO desktop application" ON)
set(WINBUTLER_EUI_NEO_SOURCE_DIR "" CACHE PATH "Use a local EUI-NEO source tree instead of fetching it")

add_library(win_butler_shell STATIC
    src/shell/src/catalog.cpp
    src/shell/src/manifest.cpp
    src/shell/src/registry.cpp
)

target_include_directories(win_butler_shell PUBLIC src/shell/include)
target_compile_definitions(win_butler_shell PUBLIC
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_compile_options(win_butler_shell PRIVATE /W4 /permissive-)
target_link_libraries(win_butler_shell PUBLIC
    advapi32
    ole32
    shell32
    shlwapi
    version
)

add_executable(win_butler_scan src/tools/scan_main.cpp)
target_link_libraries(win_butler_scan PRIVATE win_butler_shell)
target_compile_options(win_butler_scan PRIVATE /W4 /permissive-)

if(WINBUTLER_BUILD_UI)
    if(WINBUTLER_EUI_NEO_SOURCE_DIR)
        if(NOT EXISTS "${WINBUTLER_EUI_NEO_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR "WINBUTLER_EUI_NEO_SOURCE_DIR does not contain EUI-NEO CMakeLists.txt")
        endif()
        add_subdirectory("${WINBUTLER_EUI_NEO_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/eui-neo")
    else()
        include(FetchContent)
        FetchContent_Declare(
            eui_neo
            GIT_REPOSITORY https://github.com/sudoevolve/EUI-NEO.git
            GIT_TAG v0.6.5
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(eui_neo)
    endif()

    add_executable(win_butler WIN32 src/app/main.cpp)
    target_link_libraries(win_butler PRIVATE win_butler_shell)
    target_compile_options(win_butler PRIVATE /W4 /permissive-)
    eui_neo_configure_app(win_butler)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(win_butler_tests tests/unit/scan_tests.cpp)
    target_link_libraries(win_butler_tests PRIVATE win_butler_shell)
    target_include_directories(win_butler_tests PRIVATE src/shell/src)
    target_compile_definitions(win_butler_tests PRIVATE WINBUTLER_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_options(win_butler_tests PRIVATE /W4 /permissive-)
    add_test(NAME win_butler_tests COMMAND win_butler_tests)
endif()

if(WINBUTLER_BUILD_FIXTURES)
    add_subdirectory(fixtures/shellex)
endif()
