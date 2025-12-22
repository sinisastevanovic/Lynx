#pragma once
#include <Lynx.h>

#include "Panels/InspectorPanel.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/Viewport.h"

namespace Lynx
{
    class EditorLayer
    {
    public:
        EditorLayer(Engine* engine);

        void OnAttach();
        void OnDetach();
        void DrawMenuBar();
        void DrawToolBar();
        void OnImGuiRender();
        void OnScenePlay();
        void OnSceneStop();

        entt::entity GetSelectedEntity() const { return m_SelectedEntity; }
        void SetSelectedEntity(entt::entity entity) { m_SelectedEntity = entity; }

    private:
        void SaveSceneAs();
        void OpenScene();
        
        Engine* m_Engine;
        Viewport m_Viewport;
        SceneHierarchyPanel m_SceneHierarchyPanel;
        InspectorPanel m_InspectorPanel;

        std::shared_ptr<Scene> m_EditorScene;
        std::shared_ptr<Scene> m_RuntimeScene;

        entt::entity m_SelectedEntity = entt::null;
    };
}

