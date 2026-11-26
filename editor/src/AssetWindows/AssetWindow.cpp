#include "AssetWindow.h"

#include <imgui.h>

#include "Lynx/Engine.h"

#include <utility>

namespace Lynx
{
    namespace
    {
        // A window that has no place yet opens this far from the main window's corner.
        constexpr float FirstOffset = 80.0f;
    }

    AssetWindow::AssetWindow(AssetHandle asset)
        : m_Asset(asset)
    {
    }

    void AssetWindow::OnImGuiRender()
    {
        if (!m_IsOpen)
            return;

        // An asset that is gone has no name to show and nothing to look at.
        const AssetMetadata metadata = Engine::Get().GetAssetRegistry().Get(m_Asset);
        if (!metadata)
        {
            m_IsOpen = false;
            return;
        }

        // A window of the operating system: it never becomes a part of the editor's main window, and
        // it has the system's own title bar and a place in the task bar. The system then moves and
        // closes it, so ImGui draws no title bar of its own into it.
        ImGuiWindowClass ownWindow;
        ownWindow.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
        ownWindow.ViewportFlagsOverrideClear = ImGuiViewportFlags_NoDecoration | ImGuiViewportFlags_NoTaskBarIcon;
        ImGui::SetNextWindowClass(&ownWindow);

        const glm::vec2 firstSize = GetFirstSize();
        const ImVec2 mainCorner = ImGui::GetMainViewport()->Pos;
        ImGui::SetNextWindowSize(ImVec2(firstSize.x, firstSize.y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(mainCorner.x + FirstOffset, mainCorner.y + FirstOffset), ImGuiCond_FirstUseEver);
        if (std::exchange(m_ComesToFront, false))
            ImGui::SetNextWindowFocus();

        // The title names the asset. What follows the three hashes is what the saved layout knows the
        // window by: the asset, so that each asset's window opens where it was left.
        const std::string title = metadata.FilePath.filename().string() + " - " + GetKindName()
            + "###AssetWindow" + std::to_string(static_cast<uint64_t>(m_Asset));

        // Not to be docked: the system's title bar is what it is moved by, which ImGui does not see.
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse;
        if (ImGui::Begin(title.c_str(), &m_IsOpen, flags))
        {
            if (!DrawContents())
                m_IsOpen = false;
        }
        ImGui::End();
    }
}
