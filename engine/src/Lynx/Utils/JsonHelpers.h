#pragma once

#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glm/detail/type_quat.hpp>

#include "Lynx/UUID.h"
#include "Lynx/UI/Core/UIGeometry.h"

namespace glm
{
    inline void to_json(nlohmann::json& j, const vec2& v) {
        j = nlohmann::json{v.x, v.y};
    }

    inline void from_json(const nlohmann::json& j, vec2& v) {
        // Using j.at(i) throws if index doesn't exist, which is safer than j[i]
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
    }

    // vec3
    inline void to_json(nlohmann::json& j, const vec3& v) {
        j = nlohmann::json{v.x, v.y, v.z};
    }

    inline void from_json(const nlohmann::json& j, vec3& v) {
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
    }

    // vec4
    inline void to_json(nlohmann::json& j, const vec4& v) {
        j = nlohmann::json{v.x, v.y, v.z, v.w};
    }

    inline void from_json(const nlohmann::json& j, vec4& v) {
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
        v.w = j.at(3).get<float>();
    }

    inline void to_json(nlohmann::json& j, const quat& q) {             
        j = nlohmann::json{q.w, q.x, q.y, q.z}; // w, x, y, z convention
    }                                                                   
                                                                    
    inline void from_json(const nlohmann::json& j, quat& q) {           
        q.w = j.at(0).get<float>();                                     
        q.x = j.at(1).get<float>();                                     
        q.y = j.at(2).get<float>();                                     
        q.z = j.at(3).get<float>();                                     
    }                                                                   
}

namespace Lynx
{
    inline void to_json(nlohmann::json& j, const UUID& id)
    {
        j = (uint64_t)id;
    }

    inline void from_json(const nlohmann::json& j, UUID& id) {                         
        id = UUID(j.get<uint64_t>());                                                  
    }

    inline void to_json(nlohmann::json& j, const UIThickness& thickness)
    {
        j = nlohmann::json{thickness.Left, thickness.Top, thickness.Right, thickness.Bottom };
    }

    inline void from_json(const nlohmann::json& j, UIThickness& thickness) {                         
        thickness.Left = j.at(0).get<float>();
        thickness.Top = j.at(1).get<float>();
        thickness.Right = j.at(2).get<float>();
        thickness.Bottom = j.at(3).get<float>();
    }
}
