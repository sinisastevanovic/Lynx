#include <Lynx.h>
#include <Lynx/Engine.h>
#include <Lynx/GameModule.h>
#include <iostream>

#include <imgui.h>

#include "EditorLayer.h"
#include "ImGui/EditorTheme.h"
#include "Lynx/Core/DllLoader.h"

int main(int argc, char** argv)
{
    std::cout << "--- Starting Editor ---" << std::endl;

    Engine engine;
    engine.Initialize(true);
    EditorTheme::ApplyTheme();
    engine.SetSceneState(SceneState::Edit);

    IGameModule* gameModule = nullptr;
    DestroyGameFunc destroyFunc = nullptr;
    
    DllHandle gameDLL = DllLoader::Load("game_dll.dll");
    if (gameDLL.IsValid())
    {
        auto createFunc = DllLoader::GetSymbol<CreateGameFunc>(gameDLL, "CreateGameModule");
        destroyFunc = DllLoader::GetSymbol<DestroyGameFunc>(gameDLL, "DestroyGameModule");
        if (createFunc)
            gameModule = createFunc();
        
        LX_CORE_INFO("Game DLL loaded.");
    }

    {
        EditorLayer editorLayer(&engine);
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
    
    if (gameDLL.IsValid())
    {
        DllLoader::Unload(gameDLL);
        LX_CORE_INFO("Game DLL unloaded.");
    }

    engine.Shutdown();
    std::cout << "--- Editor Finished ---" << std::endl;
    return 0;
}