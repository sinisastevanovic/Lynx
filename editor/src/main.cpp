#include <Lynx.h>
#include <Lynx/Engine.h>
#include <Lynx/GameModule.h>
#include <iostream>

#include <imgui.h>

#include "EditorLayer.h"
#include "Panels/SceneHierarchyPanel.h"


#if defined(_WIN32)
    #define NOMINMAX
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

int main(int argc, char** argv)
{
    std::cout << "--- Starting Editor ---" << std::endl;

    Lynx::Engine engine;
    engine.Initialize(true);
    engine.SetSceneState(Lynx::SceneState::Edit);

    Lynx::IGameModule* gameModule = nullptr;
    Lynx::DestroyGameFunc destroyFunc = nullptr;

#if defined(_WIN32)
    HMODULE gameDLL = LoadLibraryA("game_dll.dll");
    if (gameDLL)
    {
        Lynx::CreateGameFunc createFunc = (Lynx::CreateGameFunc)GetProcAddress(gameDLL, "CreateGameModule");
        destroyFunc = (Lynx::DestroyGameFunc)GetProcAddress(gameDLL, "DestroyGameModule");
        if (createFunc)
        {
            gameModule = createFunc();
        }
    }
    else
    {
        LX_CORE_ERROR("Could not load game_dll.dll");
    }
#else
    void* gameDLL = dlopen("./libgame_dll.so", RTLD_LAZY);
    if (gameDLL)
    {
        Lynx::CreateGameFunc createFunc = (Lynx::CreateGameFunc)dlsym(gameDLL, "CreateGameModule");
        destroyFunc = (Lynx::DestroyGameFunc)dlsym(gameDLL, "DestroyGameModule");
        if (createFunc)
        {
            gameModule = createFunc();
        }
    }
    else
    {
        LX_CORE_ERROR("Could not load libgame_dll.dll");
    }
#endif

    {
        Lynx::EditorLayer editorLayer(&engine);
        editorLayer.OnAttach();

        engine.SetImGuiRenderCallback([&editorLayer]()
        {
            editorLayer.DrawMenuBar();
            ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
            
            editorLayer.OnImGuiRender();
        });
        
        engine.Run(gameModule);

        editorLayer.OnDetach();
    }
    
    engine.ClearActiveScene();
    engine.GetAssetManager().UnloadAllGameAssets();

    if (gameModule && destroyFunc)
    {
        engine.ClearGameTypes();
        destroyFunc(gameModule);
        gameModule = nullptr;
        LX_CORE_INFO("Game module destroyed.");
    }
    
#if defined(_WIN32)
    if (gameDLL)
    {
        FreeLibrary(gameDLL);
        LX_CORE_INFO("Game DLL unloaded.");
    }
#endif

    engine.Shutdown();
    std::cout << "--- Editor Finished ---" << std::endl;
    return 0;
}