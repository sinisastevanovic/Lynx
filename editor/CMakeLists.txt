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
    msdf-atlas-gen
)

target_include_directories(editor PRIVATE
    ${imgui_SOURCE_DIR}/backends
)