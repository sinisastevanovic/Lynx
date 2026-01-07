#pragma once
#include <Lynx.h>

#include "Panels/EditorPanel.h"

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

    private:
        void NewScene();
        void SaveSceneAs();
        void SaveScene();
        void OpenScene();
        void OpenScene(AssetHandle handle);

        void OnSceneContextChanged(Scene* scene);
        void OnSelectedEntityChanged(entt::entity entity);
        void OnSelectedAssetChanged(AssetHandle asset);
        void OnSelectedUIElementChanged(std::shared_ptr<UIElement> uiElement);
        
        Engine* m_Engine;

        std::vector<std::unique_ptr<EditorPanel>> m_Panels;

        std::shared_ptr<Scene> m_EditorScene;
        std::shared_ptr<Scene> m_RuntimeScene;

        entt::entity m_SelectedEntity = entt::null;
        std::shared_ptr<UIElement> m_SelectedUIElement = nullptr;
        AssetHandle m_SelectedAsset = AssetHandle::Null();
    };
}

