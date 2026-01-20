#include "SceneSerializer.h"

#include <nlohmann/json.hpp>

#include "Entity.h"
#include "Components/Components.h"
#include "Components/IDComponent.h"
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
        bool result = DeserializeFromString(ss.str());
        m_Scene->PostDeserialize();
        return result;
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

        std::unordered_map<uint64_t, entt::entity> uuidMap;

        auto entities = sceneJson["Entities"];
        if (entities.is_array())
        {
            for (auto& entityJson : entities)
            {
                uint64_t uuid = entityJson["ID"].get<uint64_t>();
                Entity newEntity = m_Scene->CreateEntity();
                auto& idComp = newEntity.GetComponent<IDComponent>();

                uuidMap[uuid] = newEntity;
                
                idComp.ID = UUID(uuid);

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

        for (auto& entityJson : entities)
        {
            uint64_t childUUID = entityJson["ID"].get<uint64_t>();

            if (entityJson.contains("Relationship"))
            {
                auto& relJson = entityJson["Relationship"];
                if (relJson.contains("Parent"))
                {
                    uint64_t parentUUID = relJson["Parent"].get<uint64_t>();
                    if (uuidMap.contains(childUUID) && uuidMap.contains(parentUUID))
                    {
                        entt::entity child = uuidMap[childUUID];
                        entt::entity parent = uuidMap[parentUUID];

                        m_Scene->AttachEntity(child, parent);
                    }
                }
            }
        }

        return true;
    }

    void SerializeEntityInternal(Entity current, nlohmann::json& outArray, bool usePrefabIDs)
    {
        auto& registry = current.GetScene()->Reg();
        nlohmann::json entityJson;

        // Determine which ID to save
        uint64_t uuidToSave = (uint64_t)current.GetUUID();
        if (usePrefabIDs && current.HasComponent<PrefabComponent>())
        {
            uuidToSave = (uint64_t)current.GetComponent<PrefabComponent>().SubEntityID;
        }
        entityJson["ID"] = uuidToSave;

        // Serialize Components
        for (const auto& [name, info] : Engine::Get().GetComponentRegistry().GetRegisteredComponents())
        {
            if (name == "Relationship")
                continue;
            if (info.has(registry, current))
            {
                if (info.serialize)
                {
                    nlohmann::json componentData;
                    info.serialize(registry, current, componentData);
                    entityJson[name] = componentData;
                }
            }
        }
        
        {
            auto& rel = current.GetComponent<RelationshipComponent>();
            if (rel.Parent != entt::null)
            {
                Entity parentEntity(rel.Parent, current.GetScene());
                UUID parentID = parentEntity.GetUUID();
                
                if (usePrefabIDs && parentEntity.HasComponent<PrefabComponent>())
                {
                    parentID = parentEntity.GetComponent<PrefabComponent>().SubEntityID;
                }
                
                nlohmann::json relJson;
                relJson["Parent"] = parentID;
                entityJson["Relationship"] = relJson;
            }
        }
        
        outArray.push_back(entityJson);

        // Follow hierarchy order
        auto& rel = current.GetComponent<RelationshipComponent>();
        entt::entity childHandle = rel.FirstChild;
        while (childHandle != entt::null)
        {
            SerializeEntityInternal(Entity(childHandle, current.GetScene()), outArray, usePrefabIDs);
            childHandle = registry.get<RelationshipComponent>(childHandle).NextSibling;
        }
    }

    nlohmann::json SceneSerializer::SerializeToJson()
    {
        nlohmann::json sceneJson;
        sceneJson["Scene"] = "Untitled"; // TODO: Add name?
        sceneJson["Entities"] = nlohmann::json::array();
        
        auto& registry = m_Scene->Reg();
        for (auto entityID : registry.view<entt::entity>())
        {
            Entity entity(entityID, m_Scene.get());
            auto& rel = entity.GetComponent<RelationshipComponent>();
            
            if (rel.Parent == entt::null)
            {
                SerializeEntityInternal(entity, sceneJson["Entities"], false);
            }
        }

        return sceneJson;
    }
    
    
    void SceneSerializer::SerializePrefab(Entity entity, nlohmann::json& outJson, bool usePrefabIDs)
    {
        outJson["Entities"] = nlohmann::json::array();
        SerializeEntityInternal(entity, outJson["Entities"], usePrefabIDs);
    }

    Entity SceneSerializer::DeserializePrefab(Scene* scene, nlohmann::json& json, Entity parent)
    {
        auto entities = json["Entities"];
        if (!entities.is_array() || entities.empty())
            return {};

        std::unordered_map<uint64_t, entt::entity> uuidMap; // Maps OLD UUID -> NEW Entity Handle
        std::unordered_map<uint64_t, uint64_t> oldToNewUUID; // Maps OLD UUID -> NEW UUID (for fixing refs)

        Entity rootEntity; // The first entity in the list is always the root (based on serialization order)

        // Pass 1: Create all entities with NEW UUIDs and deserialize components
        bool isFirst = true;
        for (auto& entityJson : entities)
        {
            uint64_t oldUUID = entityJson["ID"].get<uint64_t>();

            // Generate NEW UUID
            Entity newEntity = scene->CreateEntity();
            uint64_t newUUID = (uint64_t)newEntity.GetUUID();

            uuidMap[oldUUID] = newEntity;
            oldToNewUUID[oldUUID] = newUUID;

            if (isFirst)
            {
                rootEntity = newEntity;
                isFirst = false;
            }

            // Deserialize Components
            const auto& registeredComponents = Engine::Get().GetComponentRegistry().GetRegisteredComponents();
            for (auto& [key, value] : entityJson.items())
            {
                if (key == "ID") continue;

                if (registeredComponents.find(key) != registeredComponents.end())
                {
                    const auto& info = registeredComponents.at(key);
                    if (info.deserialize)
                    {
                        info.add(scene->Reg(), newEntity);
                        info.deserialize(scene->Reg(), newEntity, value);
                    }
                }
            }
        }

        // Pass 2: Reconstruct Hierarchy (Fix Parent/Child)
        for (auto& entityJson : entities)
        {
            uint64_t oldChildUUID = entityJson["ID"].get<uint64_t>();

            if (entityJson.contains("Relationship"))
            {
                auto& relJson = entityJson["Relationship"];
                if (relJson.contains("Parent"))
                {
                    uint64_t oldParentUUID = relJson["Parent"].get<uint64_t>();

                    // Only link if both exist in this prefab (internal link)
                    if (uuidMap.contains(oldChildUUID) && uuidMap.contains(oldParentUUID))
                    {
                        entt::entity child = uuidMap[oldChildUUID];
                        entt::entity parent = uuidMap[oldParentUUID];
                        scene->AttachEntity(child, parent);
                    }
                }
            }
        }

        // Attach root to the requested parent (if any)
        if (parent && rootEntity)
        {
            scene->AttachEntity(rootEntity, parent);
        }

        return rootEntity;
    }

    void SceneSerializer::DeserializePrefabInto(Scene* scene, nlohmann::json& json, Entity root)
    {
        auto entities = json["Entities"];
        if (!entities.is_array())
            return;
        
        AssetHandle prefabHandle = AssetHandle::Null();
        if (root.HasComponent<PrefabComponent>())
            prefabHandle = root.GetComponent<PrefabComponent>().Prefab.Handle;
        
        // 1. Map existing prefab children
        std::unordered_map<uint64_t, Entity> existingPrefabEntities;
        std::vector<Entity> staleEntities;
        
        std::function<void(Entity)> mapHierarchy = [&](Entity current)
        {
            if (current.HasComponent<PrefabComponent>())
            {
                auto& pc = current.GetComponent<PrefabComponent>();
                if (pc.Prefab.Handle == prefabHandle)
                {
                    existingPrefabEntities[(uint64_t)pc.SubEntityID] = current;
                }
            }

            // Recurse
            auto& rel = current.GetComponent<RelationshipComponent>();
            entt::entity child = rel.FirstChild;
            while (child != entt::null)
            {
                mapHierarchy(Entity(child, scene));
                child = scene->Reg().get<RelationshipComponent>(child).NextSibling;
            }
        };
        mapHierarchy(root);
        
        // 2. Deserialize & Update
        std::unordered_map<UUID, Entity> loadedMap;
        
        bool isFirst = true;
        const auto registeredComponents = Engine::Get().GetComponentRegistry().GetRegisteredComponents();
        for (auto& entityJson : entities)
        {
            UUID assetID = entityJson["ID"].get<UUID>();
            Entity currentEntity;
            if (existingPrefabEntities.contains(assetID))
            {
                // Remove all non essential existing components
                currentEntity = existingPrefabEntities[assetID];
                for (auto [name, info] : registeredComponents)
                {
                    if (!info.InternalUseOnly || name == "UICanvas")
                        info.remove(scene->Reg(), currentEntity);
                }
            }
            else
            {
                // Create new (new child added to prefab)
                currentEntity = scene->CreateEntity();
                auto& pc = currentEntity.AddComponent<PrefabComponent>();
                pc.Prefab = AssetRef<Prefab>(prefabHandle);
                pc.SubEntityID = assetID;
            }
            
            loadedMap[assetID] = currentEntity;
            existingPrefabEntities.erase(assetID);
            for (auto& [key, value] : entityJson.items())
            {
                if (key == "ID" || key == "Relationship")
                    continue;
                
                if (registeredComponents.find(key) != registeredComponents.end())
                {
                    const auto& info = registeredComponents.at(key);
                    if (info.deserialize)
                    {
                        info.add(scene->Reg(), currentEntity);
                        info.deserialize(scene->Reg(), currentEntity, value);
                    }
                }
            }
        }
        
        // 3. Fix Hierarchy
        for (auto& entityJson : entities)
        {
            UUID assetID = entityJson["ID"].get<UUID>();
            Entity childEntity = loadedMap[assetID];

            if (entityJson.contains("Relationship"))
            {
                auto& relJson = entityJson["Relationship"];
                if (relJson.contains("Parent"))
                {
                    UUID parentAssetID = relJson["Parent"].get<UUID>();
                    if (loadedMap.contains(parentAssetID))
                    {
                        Entity parentEntity = loadedMap[parentAssetID];

                        if (childEntity.GetParent() != parentEntity)
                        {
                            scene->AttachEntity(childEntity, parentEntity);
                        }
                    }
                }
            }
        }
        
        // 4. Delete Stale Entities
        for (auto& [id, entity] : existingPrefabEntities)
        {
            // Don't delete the root even if ID mismatched (safety), though matching IDs is critical.
            if (entity != root)
                scene->DestroyEntity(entity);
        }
    }
}
