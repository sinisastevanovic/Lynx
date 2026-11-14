#include "CoreComponentInspectors.h"

#include <Lynx.h>
#include <Lynx/TypeRegistry.h>
#include <Lynx/Asset/Script.h>
#include <Lynx/ImGui/LXUI.h>
#include <Lynx/Scene/Components/LuaScriptComponent.h>
#include <Lynx/Scene/Components/NativeScriptComponent.h>
#include <Lynx/Scene/Components/ParticleComponents.h>
#include <Lynx/Scene/Components/UIComponents.h>

#include <imgui.h>

namespace Lynx
{
    void RegisterCoreComponentInspectors(TypeRegistry& registry)
    {
        registry.SetComponentInspector("Camera",
            [](entt::registry& reg, entt::entity entity)
            {
                auto& cameraComp = reg.get<CameraComponent>(entity);
                LXUI::DrawCheckBox("Primary", cameraComp.Primary);
                LXUI::DrawCheckBox("FixedAspectRatio", cameraComp.FixedAspectRatio);

                std::vector<std::string> projectionTypeStrings = {"Perspective", "Orthographic"};
                int currentProjectionType = (int)cameraComp.Camera.GetProjectionType();
                if (LXUI::DrawComboControl("Projection", currentProjectionType, projectionTypeStrings))
                {
                    cameraComp.Camera.SetProjectionType((SceneCamera::ProjectionType)currentProjectionType);
                }

                if (cameraComp.Camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
                {
                    float currFOV = glm::degrees(cameraComp.Camera.GetPerspectiveVerticalFOV());
                    float currNear = cameraComp.Camera.GetPerspectiveNearClip();
                    float currFar = cameraComp.Camera.GetPerspectiveFarClip();

                    if (LXUI::DrawDragFloat("FOV", currFOV, 0.1f))
                    {
                        cameraComp.Camera.SetPerspectiveVerticalFOV(glm::radians(currFOV));
                    }
                    if (LXUI::DrawDragFloat("Near Clip", currNear, 0.1f))
                    {
                        cameraComp.Camera.SetPerspectiveNearClip(currNear);
                    }
                    if (LXUI::DrawDragFloat("Far Clip", currFar, 0.1f))
                    {
                        cameraComp.Camera.SetPerspectiveFarClip(currFar);
                    }
                }
                else
                {
                    float currSize = cameraComp.Camera.GetOrthographicSize();
                    float currNear = cameraComp.Camera.GetOrthographicNearClip();
                    float currFar = cameraComp.Camera.GetOrthographicFarClip();
                    if (LXUI::DrawDragFloat("Size", currSize, 0.1f))
                    {
                        cameraComp.Camera.SetOrthographicSize(currSize);
                    }
                    if (LXUI::DrawDragFloat("Near Clip", currNear, 0.1f))
                    {
                        cameraComp.Camera.SetOrthographicNearClip(currNear);
                    }
                    if (LXUI::DrawDragFloat("Far Clip", currFar, 0.1f))
                    {
                        cameraComp.Camera.SetOrthographicFarClip(currFar);
                    }
                }
            });

        registry.SetComponentInspector("Mesh",
            [](entt::registry& reg, entt::entity entity)
            {
                auto& meshComp = reg.get<MeshComponent>(entity);

                LXUI::DrawAssetReference("Static Mesh", meshComp.Mesh, {AssetType::StaticMesh});
                if (meshComp.Mesh.Get())
                {
                    auto& submeshes = meshComp.Mesh->GetSubmeshes();
                    int index = 0;
                    for (auto& submesh : submeshes)
                    {
                        if (submesh.Material)
                        {
                            // TODO: Change this to work with the new table layout
                            // TODO: actually change static meshes to use proper materials, not import generated ones!
                            std::string matId = "Material " + std::to_string(index);
                            ImGui::PushID(matId.c_str());

                            ImGui::Text(matId.c_str());
                            ImGui::ColorEdit4("Albedo Color", &submesh.Material->AlbedoColor.r);
                            ImGui::DragFloat("Metallic", &submesh.Material->Metallic);
                            ImGui::DragFloat("Roughness", &submesh.Material->Roughness);
                            ImGui::ColorEdit3("Emissive Color", &submesh.Material->EmissiveColor.r);
                            ImGui::DragFloat("Emissive Strength", &submesh.Material->EmissiveStrength);
                            ImGui::Separator();

                            ImGui::PopID();
                        }
                        index++;
                    }
                }
            });

        registry.SetComponentInspector("NativeScript",
            [](entt::registry& reg, entt::entity entity)
            {
                auto& nscComp = reg.get<NativeScriptComponent>(entity);

                LXUI::DrawLabel("Script Class");
                std::string current = nscComp.ScriptName.empty() ? "None" : nscComp.ScriptName;
                if (ImGui::BeginCombo("##Script Class", current.c_str()))
                {
                    for (const auto& [name, bindFunc] : Engine::Get().GetComponentRegistry().GetRegisteredScripts())
                    {
                        bool isSelected = current == name;
                        if (ImGui::Selectable(name.c_str(), isSelected))
                        {
                            nscComp.ScriptName = name;
                            Engine::Get().GetComponentRegistry().BindScript(name, nscComp);
                        }
                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            });

        registry.SetComponentInspector("LuaScript",
            [](entt::registry& reg, entt::entity entity)
            {
                auto& comp = reg.get<LuaScriptComponent>(entity);
                Scene* scene = reg.ctx().get<Scene*>();
                Entity e{ entity, scene };

                for (int i = 0; i < comp.Scripts.size(); ++i)
                {
                    auto& instance = comp.Scripts[i];
                    std::string headerName;
                    if (instance.ScriptAsset && instance.ScriptAsset.Get())
                    {
                        std::filesystem::path path = instance.ScriptAsset.Get()->GetFilePath();
                        headerName = path.stem().string();
                    }
                    else
                    {
                        headerName = "Empty Script";
                    }

                    headerName += "##" + std::to_string(i);


                    bool open = LXUI::PropertyGroup(headerName.c_str(), true);

                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("Remove Script"))
                        {
                            comp.Scripts.erase(comp.Scripts.begin() + i); // TODO: Is this sufficient?
                            i--;
                            ImGui::EndPopup();
                            continue;
                        }
                        ImGui::EndPopup();
                    }

                    if (open)
                    {
                        AssetHandle handle = instance.ScriptAsset.Handle;
                        if (LXUI::DrawAssetReference("Script Asset", handle, {AssetType::Script}))
                        {
                            instance.ScriptAsset = AssetRef<Script>(handle);
                            if (scene) // TODO: Check if properties are working correctly
                            {
                                Engine::Get().GetScriptEngine()->InstantiateScript(e, instance);
                            }
                        }

                        if (instance.Self.valid())
                        {
                            LXUI::DrawLuaScriptSection(instance, scene);
                        }
                    }
                }

                if (LXUI::DrawButton("Add Script"))
                {
                    comp.Scripts.push_back({});
                }

            });

        registry.SetComponentInspector("ParticleEmitter",
            [](entt::registry& reg, entt::entity entity)
            {
                auto& comp = reg.get<ParticleEmitterComponent>(entity);
                LXUI::DrawAssetReference("Material", comp.Material, {AssetType::Material});

                int maxP = (int)comp.MaxParticles;
                if (LXUI::DrawDragInt("Max Particles", maxP, 1.0f, 1, 100000, 1))
                {
                    comp.SetMaxParticles(maxP);
                }

                LXUI::DrawDragFloat("Emission Rate", comp.EmissionRate, 0.1f, 0.0f, 1000.0f);
                if (LXUI::DrawCheckBox("Looping", comp.IsLooping))
                {
                    comp.BurstDone = false;
                }

                if (!comp.IsLooping)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Play Burst"))
                    {
                        comp.BurstDone = false;
                    }
                }

                LXUI::DrawCheckBox("Depth Sorting", comp.DepthSorting);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (ImGui::TreeNodeEx("Properties", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    LXUI::DrawVec3Control("Position Offset", comp.Properties.Position);
                    LXUI::DrawVec3Control("Velocity", comp.Properties.Velocity);
                    LXUI::DrawVec3Control("Velocity Variation", comp.Properties.VelocityVariation);

                    LXUI::DrawColorControl("Color Begin", comp.Properties.ColorBegin);
                    LXUI::DrawColorControl("Color End", comp.Properties.ColorEnd);

                    LXUI::DrawDragFloat("Size Begin", comp.Properties.SizeBegin, 0.1f);
                    LXUI::DrawDragFloat("Size End", comp.Properties.SizeEnd, 0.1f);
                    LXUI::DrawDragFloat("Size Variation", comp.Properties.SizeVariation, 0.1f);

                    LXUI::DrawDragFloat("Life Time", comp.Properties.LifeTime, 0.1f, 0.0f, 100.0f);

                    ImGui::TreePop();
                }
            });

        registry.SetComponentInspector("UICanvas",
            [](entt::registry& reg, entt::entity entity)
            {
                auto& comp = reg.get_or_emplace<UICanvasComponent>(entity);
                LXUI::DrawDragInt("Sorting Order", comp.SortingOrder, 1, 0, 1000);
            });
    }
}
