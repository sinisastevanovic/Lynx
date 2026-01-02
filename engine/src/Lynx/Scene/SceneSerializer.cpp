#include "SceneSerializer.h"

#include <nlohmann/json.hpp>

#include "Entity.h"
#include "Components/Components.h"
#include "Lynx/Engine.h"

namespace Lynx
{
    SceneSerializer::SceneSerializer(const std::shared_ptr<Scene>& scene)
        : m_Scene(scene)
    {
    }

    void SceneSerializer::Serialize(const std::string& filepath)
    {
        std::ofstream fout(filepath);
        fout << SerializeToJson().dump(4);
    }

    std::string SceneSerializer::SerializeToString()
    {
        return SerializeToJson().dump();
    }

    bool SceneSerializer::Deserialize(const std::string& filepath)
    {
        std::ifstream stream(filepath);
        if (!stream.is_open())
        {
            LX_CORE_ERROR("Could not open file: {}", filepath);
            return false;
        }

        std::stringstream ss;
        ss << stream.rdbuf();
        return DeserializeFromString(ss.str());
    }

    bool SceneSerializer::DeserializeFromString(const std::string& serialized)
    {
        nlohmann::json sceneJson;
        try
        {
            sceneJson = nlohmann::json::parse(serialized);
        }
        catch (nlohmann::json::parse_error& e)
        {
            LX_CORE_ERROR("JSON Parse Error: {}", e.what());
            return false;
        }

        std::string sceneName = sceneJson["Scene"];
        LX_CORE_INFO("Deserializing scene: {}", sceneName);

        auto entities = sceneJson["Entities"];
        if (entities.is_array())
        {
            for (auto& entityJson : entities)
            {
                Entity newEntity = m_Scene->CreateEntity();
                auto& idComp = newEntity.GetComponent<IDComponent>();
                idComp.ID = UUID(entityJson["ID"].get<uint64_t>());

                const auto& registeredComponents = Engine::Get().GetComponentRegistry().GetRegisteredComponents();
                for (auto& [key, value] : entityJson.items())
                {
                    if (registeredComponents.find(key) != registeredComponents.end())
                    {
                        const auto& info = registeredComponents.at(key);
                        if (info.deserialize)
                        {
                            info.add(m_Scene->Reg(), newEntity);
                            info.deserialize(m_Scene->Reg(), newEntity, value);
                        }
                    }
                }
            }
        }

        return true;
    }

    nlohmann::json SceneSerializer::SerializeToJson()
    {
        nlohmann::json sceneJson;
        sceneJson["Scene"] = "Untitled"; // TODO: Add name?
        sceneJson["Entities"] = nlohmann::json::array();

        auto& registry = m_Scene->Reg();
        for (auto entity : registry.view<entt::entity>())
        {
            nlohmann::json entityJson;
            auto& idComp = registry.get<IDComponent>(entity);
            entityJson["ID"] = (uint64_t)idComp.ID;

            for (const auto& [name, info] : Engine::Get().GetComponentRegistry().GetRegisteredComponents())
            {
                if (info.has(registry, entity))
                {
                    if (info.serialize)
                    {
                        nlohmann::json componentData;
                        info.serialize(registry, entity, componentData);
                        entityJson[name] = componentData;
                    }
                }
            }
            sceneJson["Entities"].push_back(entityJson);
        }

        return sceneJson;
    }
}
