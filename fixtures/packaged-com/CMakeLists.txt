cmake_minimum_required(VERSION 3.24)

project(win_butler_packaged_com_fixture LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Packaged COM fixture targets Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Build the packaged COM fixture with -A x64 because Explorer is 64-bit on x64 Windows.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(win_butler_fixture_explorer_command SHARED fixture_explorer_command.cpp)
set_target_properties(win_butler_fixture_explorer_command PROPERTIES
    OUTPUT_NAME "win_butler_fixture_explorer_command"
    PREFIX ""
)
target_compile_definitions(win_butler_fixture_explorer_command PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_compile_options(win_butler_fixture_explorer_command PRIVATE /W4 /permissive-)
target_link_libraries(win_butler_fixture_explorer_command PRIVATE
    ole32
    shell32
    shlwapi
    user32
)
target_sources(win_butler_fixture_explorer_command PRIVATE fixture_explorer_command.def)

add_executable(win_butler_fixture_host WIN32 fixture_host.cpp)
target_compile_definitions(win_butler_fixture_host PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_compile_options(win_butler_fixture_host PRIVATE /W4 /permissive-)
target_link_libraries(win_butler_fixture_host PRIVATE user32)
