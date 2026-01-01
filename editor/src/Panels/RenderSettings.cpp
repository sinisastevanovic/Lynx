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
            if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Enabled", &bloom.Enabled);
                ImGui::DragFloat("Threshold", &bloom.Threshold, 0.1f, 0.0f, 20.0f);
                ImGui::DragFloat("Knee", &bloom.Knee, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Radius", &bloom.Radius, 0.01f, 0.1f, 100.0f);
                ImGui::DragFloat("Intensity", &bloom.Intensity, 0.001f, 0.0f, 1.0f);
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
