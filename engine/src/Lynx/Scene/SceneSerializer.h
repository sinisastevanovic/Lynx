#pragma once
#include "Lynx/Scene/Scene.h"
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

    private:
        nlohmann::json SerializeToJson();
        
        std::shared_ptr<Scene> m_Scene;
    };
}

