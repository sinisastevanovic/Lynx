add_executable(game_standalone
    src/main.cpp
)

set_target_properties(game_standalone PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(game_standalone PRIVATE game)

add_dependencies(game_standalone engine)