#include "SceneHierarchyPanel.h"

#include <imgui.h>

#include "Lynx/Scene/Entity.h"
#include "Lynx/Scene/Components/Components.h"
#include "Lynx/Scene/Components/UIComponents.h"
#include "Lynx/UI/Widgets/GridPanel.h"
#include "Lynx/UI/Widgets/StackPanel.h"
#include "Lynx/UI/Widgets/UIButton.h"
#include "Lynx/UI/Widgets/UIImage.h"
#include "Lynx/UI/Widgets/UIScrollView.h"
#include "Lynx/UI/Widgets/UIText.h"
#include "Lynx/Utils/JsonHelpers.h"

namespace Lynx
{
    void SceneHierarchyPanel::OnImGuiRender()
    {
        ImGui::Begin("Scene Hierarchy");

        if (m_Context)
        {
            std::string sceneName;
            if (!m_Context->GetFilePath().empty())
            {
                std::filesystem::path filePath = m_Context->GetFilePath();
                sceneName = filePath.stem().string();
            }
            else
            {
                sceneName = "Untitled";
            }

            ImGui::Text("%s", sceneName.c_str());
            ImGui::Separator();
            
            for (auto entity : m_Context->Reg().view<entt::entity>())
            {
                auto& rel = m_Context->Reg().get<RelationshipComponent>(entity);
                if (rel.Parent == entt::null)
                    DrawEntityNode(entity, m_Selection == entity);
            }

            if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
            {
                if (ImGui::MenuItem("Create Empty Entity"))
                    m_Context->CreateEntity("Empty Entity");

                ImGui::EndPopup();
            }

            if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered() && OnSelectionChangedCallback)
            {
                OnSelectionChangedCallback(entt::null);
                OnSelectedUIElementChangedCallback(nullptr);
                m_SelectedUIElement = nullptr;
            }

            ImGui::Dummy(ImGui::GetContentRegionAvail());
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY"))
                {
                    entt::entity payloadEntity = *(const entt::entity*)payload->Data;
                    m_Context->DetachEntityKeepWorld(payloadEntity);
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsWindowFocused() && m_Selection != entt::null)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_Delete))
                {
                    m_DeferredDeletion = m_Selection;
                }
            }
        }
        
        ImGui::End();

        if (m_DeferredDeletion != entt::null)
        {
            if (m_Context->Reg().valid(m_DeferredDeletion))
            {
                m_Context->DestroyEntity(m_DeferredDeletion);
                if (m_Selection == m_DeferredDeletion)
                    OnSelectionChangedCallback(entt::null);
            }
            m_DeferredDeletion = entt::null;
        }
    }

    void SceneHierarchyPanel::OnSceneContextChanged(Scene* context)
    {
        m_Context = context;
        m_Selection = entt::null;
    }

    void SceneHierarchyPanel::OnSelectedEntityChanged(entt::entity selectedEntity)
    {
        m_SelectedUIElement = nullptr;
        m_Selection = selectedEntity;
    }

    void SceneHierarchyPanel::DrawEntityNode(entt::entity entity, bool isSelected)
    {
        auto& tag = m_Context->Reg().get<TagComponent>(entity).Tag;

        ImGuiTreeNodeFlags flags = (isSelected ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
        flags |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
        flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

        bool hasChildren = (m_Context->Reg().get<RelationshipComponent>(entity).ChildrenCount > 0) || m_Context->Reg().all_of<UICanvasComponent>(entity);
        if (!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf;

        bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Delete Entity"))
            {
                m_DeferredDeletion = entity;
            }

            ImGui::EndPopup();
        }

        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload("SCENE_HIERARCHY_ENTITY", &entity, sizeof(entt::entity));
            ImGui::Text("%s", tag.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY"))
            {
                entt::entity payloadEntity = *(const entt::entity*)payload->Data;
                if (payloadEntity != entity)
                {
                    m_Context->AttachEntityKeepWorld(payloadEntity, entity);
                }
            }
            ImGui::EndDragDropTarget();
        }
        
        if (ImGui::IsItemClicked() && OnSelectionChangedCallback)
        {
            OnSelectedUIElementChangedCallback(nullptr);
            m_SelectedUIElement = nullptr;
            OnSelectionChangedCallback(entity);
        }

        if (opened)
        {
            if (hasChildren)
            {
                auto& rel = m_Context->Reg().get<RelationshipComponent>(entity);
                entt::entity currentChild = rel.FirstChild;

                while (currentChild != entt::null)
                {
                    DrawEntityNode(currentChild, m_Selection == currentChild);
                    currentChild = m_Context->Reg().get<RelationshipComponent>(currentChild).NextSibling;
                }
            }

            if (m_Context->Reg().all_of<UICanvasComponent>(entity))
            {
                auto& canvas = m_Context->Reg().get<UICanvasComponent>(entity).Canvas;
                if (canvas)
                {
                    DrawUINode(canvas);
                }
            }
           
            ImGui::TreePop();
        }
    }

    void SceneHierarchyPanel::DrawUINode(std::shared_ptr<UIElement> element)
    {
        if (!element)
            return;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

        if (m_SelectedUIElement == element)
            flags |= ImGuiTreeNodeFlags_Selected;

        if (element->GetChildren().empty())
            flags |= ImGuiTreeNodeFlags_Leaf;

        bool opened = ImGui::TreeNodeEx((void*)element.get(), flags, element->GetName().c_str());
        
        if (ImGui::BeginDragDropSource())
        {
            UIElement* ptr = element.get();
            ImGui::SetDragDropPayload("HIERARCHY_UI_ELEMENT", &ptr, sizeof(UIElement*));
            ImGui::Text("%s", element->GetName().c_str());
            ImGui::EndDragDropSource();
        }
        
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_UI_ELEMENT"))
            {
                UIElement* ptr = *(UIElement**)payload->Data;
                std::shared_ptr<UIElement> source = ptr->shared_from_this();
                
                if (source && source != element && !element->IsDescendantOf(source))
                {
                    if (auto oldParent = source->GetParent())
                        oldParent->RemoveChild(source);
                    
                    element->AddChild(source);
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::BeginMenu("Create"))
            {
                if (ImGui::MenuItem("Image"))
                {
                    auto child = std::make_shared<UIImage>();
                    child->SetName("Image");
                    element->AddChild(child);
                }
                if (ImGui::MenuItem("Text"))
                {
                    auto child = std::make_shared<UIText>();
                    child->SetName("Text");
                    element->AddChild(child);
                }
                if (ImGui::MenuItem("Button"))
                {
                    auto child = std::make_shared<UIButton>();
                    child->SetName("Button");
                    element->AddChild(child);
                }
                if (ImGui::MenuItem("Text Button"))
                {
                    auto child = std::make_shared<UIButton>();
                    child->SetName("TextButton");
                    element->AddChild(child);

                    auto label = std::make_shared<UIText>();
                    label->SetName("Text");
                    label->SetSize({ 0.0, 0.0 });
                    label->SetAnchor(UIAnchor::StretchAll);
                    label->SetTextAlignment(TextAlignment::Center);
                    label->SetVerticalAlignment(TextVerticalAlignment::Center);
                    child->AddChild(label);
                }
                if (ImGui::MenuItem("Stack Panel"))
                {
                    auto child = std::make_shared<StackPanel>();
                    child->SetName("Stack Panel");
                    element->AddChild(child);
                }
                if (ImGui::MenuItem("Grid Panel"))
                {
                    auto child = std::make_shared<GridPanel>();
                    child->SetName("Grid Panel");
                    element->AddChild(child);
                }
                if (ImGui::MenuItem("Scroll Viewer"))
                {
                    auto child = std::make_shared<UIScrollView>();
                    child->SetName("Scroll Viewer");
                    element->AddChild(child);
                    
                    auto vScrollbar = std::make_shared<UIImage>();
                    vScrollbar->SetName("Vertical Scrollbar");
                    vScrollbar->SetPivot({1.0f, 0.5f});
                    vScrollbar->SetSize({10.0f, 0.0f });
                    vScrollbar->SetTint({ 0.5f, 0.5f, 0.5f, 0.5f });
                    child->AddChild(vScrollbar);
                    child->SetVerticalScrollbar(vScrollbar);
                }
                if (ImGui::MenuItem("Empty"))
                {
                    auto child = std::make_shared<UIElement>();
                    child->SetName("Element");
                    element->AddChild(child);
                }
                ImGui::EndMenu();
            }

            bool hasParent = element->GetParent() != nullptr;
            if (!hasParent)
                ImGui::BeginDisabled(true);
            if (ImGui::MenuItem("Duplicate"))
            {
                nlohmann::json data;
                element->Serialize(data);
                data["UUID"] = UUID();
                
                std::shared_ptr<UIElement> clone = UIElement::CreateFromType(data["Type"]);
                clone->Deserialize(data);
                
                if (auto parent = element->GetParent())
                    parent->AddChild(clone);
                
            }
            if (ImGui::MenuItem("Delete"))
            {
                // TODO: reparent children?
                element->GetParent()->RemoveChild(element);
                if (m_SelectedUIElement == element)
                {
                    m_SelectedUIElement = nullptr;
                    OnSelectedUIElementChangedCallback(nullptr);
                }
            }
            if (!hasParent)
                ImGui::EndDisabled();

            ImGui::EndPopup();
        }

       // if (ImGui::IsItemClicked() && OnSelectedUIElementChangedCallback)
        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && OnSelectedUIElementChangedCallback)
        {
            if (OnSelectionChangedCallback)
                OnSelectionChangedCallback(entt::null);
            m_SelectedUIElement = element;
            OnSelectedUIElementChangedCallback(element);
        }

        if (opened)
        {
            bool hasPayload = ImGui::GetDragDropPayload() && std::string(ImGui::GetDragDropPayload()->DataType) == "HIERARCHY_UI_ELEMENT";
            const auto& children = element->GetChildren();
            for (size_t i = 0; i < children.size(); i++)
            {
                auto child = children[i];
                
                if (hasPayload)
                {
                    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 4.0f));
                
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_UI_ELEMENT"))
                        {
                            UIElement* sourcePtr = *(UIElement**)payload->Data;
                            auto source = sourcePtr->shared_from_this();

                            if (source->GetParent() == element)
                            {
                                element->MoveChild(source, i);
                            }
                            else
                            {
                                source->GetParent()->RemoveChild(source);
                                element->AddChildAt(source, i);
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
                
                DrawUINode(child);
            }
            
            if (hasPayload && children.size() > 0)
            {
                ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 4.0f));
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_UI_ELEMENT"))
                    {
                        UIElement* sourcePtr = *(UIElement**)payload->Data;
                        auto source = sourcePtr->shared_from_this();
                        if (source->GetParent() == element)
                        {
                            element->MoveChild(source, element->GetChildren().size());
                        }
                        else
                        {
                            source->GetParent()->RemoveChild(source);
                            element->AddChild(source);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }
            ImGui::TreePop();
        }
    }
}
