#include "RenderSettings.h"

#include <imgui.h>

#include "Lynx/Engine.h"

namespace Lynx
{
    void RenderSettings::OnImGuiRender()
    {
        if (ImGui::Begin("Renderer Settings"))
        {
            auto& renderer = Engine::Get().GetRenderer();
            auto& bloom = renderer.GetBloomSettings();

            if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen))
            {
                bool vsync = renderer.IsVSync();
                if (ImGui::Checkbox("VSync", &vsync))
                    renderer.SetVSync(vsync);

                bool extraImage = renderer.HasExtraSwapchainImage();
                if (ImGui::Checkbox("Extra swapchain image", &extraImage))
                    renderer.SetExtraSwapchainImage(extraImage);
                ImGui::SetItemTooltip("One window image more than the driver's minimum.\nWith VSync the CPU can start the next frame without waiting for the display.");
            }

            if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Enabled", &bloom.Enabled);
                ImGui::DragFloat("Threshold", &bloom.Threshold, 0.1f, 0.0f, 20.0f);
                ImGui::DragFloat("Knee", &bloom.Knee, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Radius", &bloom.Radius, 0.01f, 0.1f, 100.0f);
                ImGui::DragFloat("Intensity", &bloom.Intensity, 0.001f, 0.0f, 1.0f);
            }

            if (ImGui::CollapsingHeader("Level of Detail", ImGuiTreeNodeFlags_DefaultOpen))
            {
                LodSettings& lods = renderer.GetLodSettings();
                ImGui::DragFloat("Error in pixels", &lods.ErrorPixels, 0.05f, 0.1f, 32.0f);
                ImGui::SetItemTooltip("A mesh is drawn in a coarser level once that level is off by no more than this on screen.");

                ImGui::DragFloat("Leaf in pixels", &lods.LeafPixels, 0.1f, 0.5f, 64.0f);
                ImGui::SetItemTooltip("Foliage is drawn with fewer and larger leaves once a leaf is at most this large on screen.");

                int shadowBias = static_cast<int>(lods.ShadowBias);
                if (ImGui::SliderInt("Coarser in shadows", &shadowBias, 0, 4))
                    lods.ShadowBias = static_cast<uint32_t>(shadowBias);

                ImGui::SliderInt("Show level", &lods.ForcedLevel, -1, static_cast<int>(MeshLod::MaxCount) - 1);
                ImGui::SetItemTooltip("Draws every mesh in this level, or in its coarsest if it has fewer.\n-1 picks the level by distance.");
            }

            if (ImGui::CollapsingHeader("AntiAliasing", ImGuiTreeNodeFlags_DefaultOpen))
            {
                bool fxaa = renderer.GetFXAAEnabled();
                if (ImGui::Checkbox("Enable FXAA", &fxaa))
                    renderer.SetFXAAEnabled(fxaa);
            }
        }
        ImGui::End();
    }
}
