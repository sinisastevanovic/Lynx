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

            if (ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ShadowSettings& shadows = renderer.GetShadowSettings();
                ImGui::DragFloat("Distance", &shadows.Distance, 1.0f, 10.0f, 5000.0f);
                ImGui::SetItemTooltip("How far from the camera the sun's shadow is drawn.");
                ImGui::Checkbox("Fit to what is seen", &shadows.FitsToView);
                ImGui::SetItemTooltip("Shares the cascades out over the stretch of the view that holds something,\ninstead of over everything from the camera to the distance.");
                ImGui::BeginDisabled(shadows.FitsToView);
                ImGui::SliderFloat("Detail near the camera", &shadows.SplitWeight, 0.0f, 1.0f);
                ImGui::SetItemTooltip("How the distance is shared out among the four cascades while they are not fitted.\nHigher gives the near ones more of the detail.");
                ImGui::EndDisabled();
                ImGui::DragFloat("Widest shadow edge", &shadows.WidestEdge, 0.01f, 0.0f, 2.0f);
                ImGui::SetItemTooltip("The widest the soft edge of a shadow gets, from its middle to its rim.");
                ImGui::DragFloat("Smallest caster in texels", &shadows.SmallestCasterInTexels, 0.1f, 0.0f, 64.0f);
                ImGui::SetItemTooltip("Things smaller than this many texels of a cascade's map cast no shadow into it.\nSmall things then lose their shadow some way off, which is where most of the cost of far shadows lies.");
            }

            if (ImGui::CollapsingHeader("Ambient Occlusion", ImGuiTreeNodeFlags_DefaultOpen))
            {
                AmbientOcclusionSettings& occlusion = renderer.GetAmbientOcclusionSettings();
                ImGui::Checkbox("Darken corners and creases", &occlusion.Enabled);
                ImGui::SetItemTooltip("Keeps the light of the sky out of where things stand in its way. Never dims the sun.");

                ImGui::DragFloat("Reach", &occlusion.Radius, 0.01f, 0.05f, 10.0f);
                ImGui::SetItemTooltip("How far around a place something counts as standing in the sky's way.");

                int quality = static_cast<int>(occlusion.Quality);
                if (ImGui::Combo("Quality", &quality, AmbientOcclusionQualityNames.data(), static_cast<int>(AmbientOcclusionQualityNames.size())))
                    occlusion.Quality = static_cast<AmbientOcclusionQuality>(quality);
                ImGui::SetItemTooltip("How many directions and places are tried per pixel and frame. Lower is grainier while the picture moves.");

                int rounds = static_cast<int>(occlusion.SmoothingRounds);
                if (ImGui::SliderInt("Smoothing rounds", &rounds, 0, 3))
                    occlusion.SmoothingRounds = static_cast<uint32_t>(rounds);
                ImGui::SetItemTooltip("How many times every pixel is mixed with those around it on the same surface. More takes more grain out of a moving picture.");
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
                TemporalSettings& temporal = renderer.GetTemporalSettings();
                ImGui::Checkbox("Temporal anti-aliasing", &temporal.Resolves);
                ImGui::SetItemTooltip("Smooths the picture over several frames.\nThe view modes that start with \"TAA\" show how it works a frame out.");

                // FXAA only smooths a frame on its own, and only stands in while the above is off.
                ImGui::BeginDisabled(temporal.Resolves);
                bool fxaa = renderer.GetFXAAEnabled();
                if (ImGui::Checkbox("FXAA instead", &fxaa))
                    renderer.SetFXAAEnabled(fxaa);
                ImGui::SetItemTooltip("A cheaper smoothing of single frames, used while temporal anti-aliasing is off.");
                ImGui::EndDisabled();
                if (temporal.Resolves)
                {
                    int jitterLength = static_cast<int>(temporal.JitterLength);
                    if (ImGui::SliderInt("Jitter length", &jitterLength, 1, 64))
                        temporal.JitterLength = static_cast<uint32_t>(jitterLength);
                    ImGui::SetItemTooltip("After this many frames the camera's shifts repeat.");

                    ImGui::SliderFloat("History time", &temporal.HistoryTime, 0.02f, 1.0f, "%.2f s");
                    ImGui::SetItemTooltip("How long what a pixel showed keeps counting. Longer is smoother and slower to follow a change.");

                    ImGui::SliderFloat("Box width", &temporal.BoxGamma, 0.0f, 3.0f);
                    ImGui::SetItemTooltip("How far the history may lie from the colours around a pixel, in standard deviations.\n0 allows all the colours around it: least flicker, longest trails.");

                    ImGui::SliderFloat("History sharpness", &temporal.Sharpness, 0.0f, 1.0f);
                    ImGui::SetItemTooltip("How sharply the history is read between pixels. 0.5 is the usual curve.");

                    ImGui::SliderFloat("Share between pixels", &temporal.BetweenPixelsShare, 0.0f, 1.0f);
                    ImGui::SetItemTooltip("How much a frame counts where the history is read halfway between pixels. Higher keeps what moves sharper and its edges rougher. 0 makes no difference.");

                    ImGui::SliderFloat("History time when still", &temporal.StillHistoryTime, 0.02f, 2.0f, "%.2f s");
                    ImGui::SetItemTooltip("The same for a pixel that stands still. Longer flickers less on thin things and follows a change of light later.");

                    ImGui::SliderFloat("Box when still", &temporal.StillBoxScale, 1.0f, 8.0f);
                    ImGui::SetItemTooltip("How many times wider the box of colours is for a pixel that stands still. Wider keeps thin things like needles from flickering.");

                    ImGui::Checkbox("New shadow grain every frame", &temporal.VariesNoise);
                    ImGui::SetItemTooltip("Draws the grain in the soft edge of a shadow anew in every frame, so that the frames average it away.");

                    ImGui::Checkbox("Steady textures", &temporal.SteadyTextures);
                    ImGui::SetItemTooltip("Reads colour textures where a pixel would be without the jitter, so their patterns stand still.");

                    ImGui::SliderFloat("Depth tolerance", &temporal.DepthTolerance, 0.0f, 1.0f);
                    ImGui::SetItemTooltip("By which share the depth of the history may differ before it counts as another surface.");
                }
            }
        }
        ImGui::End();
    }
}
