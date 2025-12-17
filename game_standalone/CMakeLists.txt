add_executable(game_standalone
    src/main.cpp
)

target_link_libraries(game_standalone PRIVATE game)

add_dependencies(game_standalone engine)