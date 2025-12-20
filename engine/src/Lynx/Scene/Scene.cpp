#include "Scene.h"
#include "Components.h"
#include "Entity.h"

namespace Lynx
{
    Scene::Scene()
    {
    }

    Scene::~Scene()
    {
    }

    Entity Scene::CreateEntity(const std::string& name)
    {
        Entity entity = { m_Registry.create(), this };
        entity.AddComponent<IDComponent>(); // TODO: UUID generation
        entity.AddComponent<TransformComponent>();
        auto& tag = entity.AddComponent<TagComponent>();
        tag.Tag = name.empty() ? "Entity" : name;
        return entity;
    }

    void Scene::DestroyEntity(entt::entity entity)
    {
        m_Registry.destroy(entity);
    }

    void Scene::OnUpdate(float ts)
    {
        // Scripts, Physics, etc. would go here
    }
}
