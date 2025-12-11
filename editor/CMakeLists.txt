file(GLOB_RECURSE EDITOR_SOURCES
    CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.h"
)

add_executable(editor ${EDITOR_SOURCES})


target_link_libraries(editor PRIVATE 
    engine
)

add_dependencies(editor engine game_dll)

add_custom_command(TARGET editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:engine>
            $<TARGET_FILE:game_dll>
            $<TARGET_FILE_DIR:editor>
    COMMENT "Copying required DLLs to editor directory"
)