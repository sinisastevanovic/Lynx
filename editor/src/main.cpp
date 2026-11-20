#include <Lynx.h>
#include <Lynx/Engine.h>
#include <Lynx/GameModule.h>
#include <iostream>

#include <imgui.h>

#include "EditorLayer.h"
#include "Components/EditorComponents.h"
#include "Inspectors/CoreComponentInspectors.h"
#include "ImGui/EditorTheme.h"
#include "Lynx/Core/CommandLine.h"
#include "Lynx/Core/DllLoader.h"

void RegisterEditorComponents(TypeRegistry& registry)
{
    registry.RegisterCoreInternalOnlyComponent<EditorMetadataComponent>("EditorMetadata",
        [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
        {
            auto& component = reg.get<EditorMetadataComponent>(entity);
            json["ComponentOrder"] = component.ComponentOrder;
        },
        [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
        {
            auto& component = reg.get_or_emplace<EditorMetadataComponent>(entity);
            component.ComponentOrder = json.value<std::vector<std::string>>("ComponentOrder", {});
        });
}

int main(int argc, char** argv)
{
    std::cout << "--- Starting Editor ---" << std::endl;

    const CommandLine commandLine(argc, argv);
    const auto project = Project::LoadFromCommandLine(commandLine);
    if (!project)
    {
        std::cerr << project.error() << std::endl;
        return 1;
    }

    Engine engine;
    engine.Initialize(*project, true);
    engine.SetProfilerOverlayVisible(commandLine.Has("profiler"));
    EditorTheme::ApplyTheme();
    
    auto& registry = engine.GetComponentRegistry();
    RegisterCoreComponentInspectors(registry);
    RegisterEditorComponents(registry);

    IGameModule* gameModule = nullptr;
    DestroyGameFunc destroyFunc = nullptr;
    
    DllHandle gameDLL;
    if (!project->GameModule.empty())
        gameDLL = DllLoader::Load(DllLoader::PlatformFileName(project->GameModule));

    if (gameDLL.IsValid())
    {
        auto createFunc = DllLoader::GetSymbol<CreateGameFunc>(gameDLL, "CreateGameModule");
        destroyFunc = DllLoader::GetSymbol<DestroyGameFunc>(gameDLL, "DestroyGameModule");
        if (createFunc)
            gameModule = createFunc();
        
        LX_CORE_INFO("Game DLL loaded.");
    }

    if (gameModule)
        engine.SetGameModule(gameModule);

    {
        EditorLayer editorLayer(&engine);
        editorLayer.OnAttach();

        engine.SetImGuiRenderCallback([&editorLayer]()
        {
            editorLayer.DrawMenuBar();
            ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
            
            editorLayer.OnImGuiRender();
        });
        
        engine.Run();

        editorLayer.OnDetach();
    }
    
    engine.PreGameShutdown();

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