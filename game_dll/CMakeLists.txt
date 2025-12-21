add_library(game_dll SHARED 
    src/dll_main.cpp
)

target_link_libraries(game_dll PRIVATE game)

target_compile_definitions(game_dll PRIVATE 
    GAME_DLL_EXPORT
    "IMGUI_API=__declspec(dllimport)"
)