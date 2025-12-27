file(GLOB_RECURSE EDITOR_SOURCES
    CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.h"
)

add_executable(editor ${EDITOR_SOURCES})

target_compile_definitions(editor PRIVATE
    "IMGUI_API=__declspec(dllimport)"
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

set_target_properties(editor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(editor PRIVATE 
    engine
)

target_include_directories(editor PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

add_dependencies(editor engine game_dll)