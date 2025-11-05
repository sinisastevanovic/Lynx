add_executable(game_standalone
    src/main.cpp
)

target_link_libraries(game_standalone PRIVATE game)

add_custom_command(TARGET game_standalone POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:engine>
        $<TARGET_FILE_DIR:game_standalone>
    COMMENT "Copying required DLLs to game directory"
)