#include "AssetWindow.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "Lynx/Engine.h"

#include <utility>

namespace Lynx
{
    namespace
    {
        // A window that has no place yet opens this far from the main window's corner.
        constexpr float FirstOffset = 80.0f;

        // Dropdown lists and menus opened from the window that is being drawn, if ImGui has moved them
        // into the editor's main window, are handed back.
        //
        // A list that sticks out of its window gets an operating system window of its own, and one
        // that fits again is folded back, but only ever into the main window: ImGui knows no other
        // window that can hold more than itself (the FIXME at UpdateTryMergeWindowIntoHostViewports).
        // Opened at the mouse, a list sticks out for its first frame whenever the click was near the
        // right or lower edge. Folded into the main window, it is drawn there, under this window,
        // where nobody sees it. Without a viewport, a list takes its parent's when it is next begun.
        void KeepPopupsInThisWindow()
        {
            ImGuiContext& context = *ImGui::GetCurrentContext();
            const ImGuiWindow* thisWindow = ImGui::GetCurrentWindow();
            const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
            if (thisWindow->Viewport == mainViewport)
                return;

            for (const ImGuiPopupData& popup : context.OpenPopupStack)
            {
                ImGuiWindow* list = popup.Window;
                if (!list || list->Viewport != mainViewport)
                    continue;

                // A list opened from another list, or from a part of the window, belongs to it as well.
                for (const ImGuiWindow* parent = list->ParentWindow; parent; parent = parent->ParentWindow)
                {
                    if (parent == thisWindow)
                    {
                        list->Viewport = nullptr;
                        list->ViewportId = 0;
                        break;
                    }
                }
            }
        }
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
            KeepPopupsInThisWindow();
            if (!DrawContents())
                m_IsOpen = false;
        }
        ImGui::End();
    }
}
