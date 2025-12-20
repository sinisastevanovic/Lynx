file(GLOB_RECURSE EDITOR_SOURCES
    CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.h"
)

add_executable(editor ${EDITOR_SOURCES})

set_target_properties(editor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(editor PRIVATE 
    engine
)

add_dependencies(editor engine game_dll)