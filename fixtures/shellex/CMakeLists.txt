add_library(win_butler_fixture_shellex SHARED fixture_shellex.cpp)
set_target_properties(win_butler_fixture_shellex PROPERTIES
    OUTPUT_NAME "win_butler_fixture_shellex"
    PREFIX ""
)
target_compile_definitions(win_butler_fixture_shellex PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_compile_options(win_butler_fixture_shellex PRIVATE /W4 /permissive-)
target_link_libraries(win_butler_fixture_shellex PRIVATE
    advapi32
    ole32
    shell32
    shlwapi
    user32
)
target_sources(win_butler_fixture_shellex PRIVATE fixture_shellex.def)
