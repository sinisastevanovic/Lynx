#pragma once
#include "Lynx/Scene/Scene.h"
#include "Lynx/Scene/Entity.h"
#include <nlohmann/json_fwd.hpp>

namespace Lynx
{
    class LX_API SceneSerializer
    {
    public:
        SceneSerializer(const std::shared_ptr<Scene>& scene);

        void Serialize(const std::string& filepath);
        std::string SerializeToString();
        bool Deserialize(const std::string& filepath);
        bool DeserializeFromString(const std::string& serialized);
        
        static void SerializePrefab(Entity entity, nlohmann::json& outJson, bool usePrefabIDs);
        static Entity DeserializePrefab(Scene* scene, nlohmann::json& json, Entity parent = {});
        static void DeserializePrefabInto(Scene* scene, nlohmann::json& json, Entity root);

    private:
        nlohmann::json SerializeToJson();
        
        std::shared_ptr<Scene> m_Scene;
    };
}

