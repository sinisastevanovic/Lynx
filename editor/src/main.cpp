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

    if (gameModule)
    {
        LX_CORE_INFO("Game DLL loaded successfully.");
        gameModule->RegisterComponents(&engine.ComponentRegistry);

        LX_CORE_TRACE("Querying registerd types:");
        for (const auto& [name, info] : engine.ComponentRegistry.GetRegisteredComponents())
        {
            LX_CORE_TRACE("  - Found component: {}", name);
        }
    }

    Lynx::EditorLayer editorLayer(&engine);
    editorLayer.OnAttach();

    Lynx::SceneHierarchyPanel sceneHierarchyPanel(engine.GetActiveScene());
    engine.SetImGuiRenderCallback([&editorLayer]()
    {
        editorLayer.DrawMenuBar();
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        
        ImGui::ShowDemoWindow();

        //editorLayer.DrawToolBar();
        editorLayer.OnImGuiRender();
    });
    
    engine.Run(gameModule);

    editorLayer.OnDetach();

    if (gameModule && destroyFunc)
    {
        destroyFunc(gameModule);
        LX_CORE_INFO("Game module destroyed.");
    }

    engine.Shutdown();
    std::cout << "--- Editor Finished ---" << std::endl;
    return 0;
}