#include <Lynx.h>
#include <Lynx/Engine.h>
#include <Lynx/GameModule.h>
#include <iostream>

#include <imgui.h>


#if defined(_WIN32)
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

int main(int argc, char** argv)
{
    std::cout << "--- Starting Editor ---" << std::endl;

    Lynx::Engine engine;
    engine.Initialize();

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

    engine.SetImGuiRenderCallback([]()
    {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        ImGui::Begin("Viewport");
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        if (viewportSize.x > 0 && viewportSize.y > 0)
            Lynx::Engine::Get().GetRenderer().EnsureEditorViewport((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);
        
        auto texture = Lynx::Engine::Get().GetRenderer().GetViewportTexture();
        if (texture)
        {
            ImGui::Image((ImTextureID)texture.Get(), viewportSize);
        }
        ImGui::End();
        
        ImGui::ShowDemoWindow();
    });
    
    engine.Run(gameModule);

    if (gameModule && destroyFunc)
    {
        destroyFunc(gameModule);
        LX_CORE_INFO("Game module destroyed.");
    }

    engine.Shutdown();
    std::cout << "--- Editor Finished ---" << std::endl;
    return 0;
}