#include "ScriptWrappers.h"

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>

namespace Lynx
{
    namespace ScriptWrappers
    {
        void RegisterMath(sol::state& lua)
        {
            lua.new_usertype<glm::vec4>("Color",
                sol::call_constructor, sol::constructors<glm::vec4(float, float, float, float), glm::vec4(float), glm::vec4()>(),

                "r", &glm::vec4::r,
                "g", &glm::vec4::g,
                "b", &glm::vec4::b,
                "a", &glm::vec4::a,

                "x", &glm::vec4::x,
                "y", &glm::vec4::y,
                "z", &glm::vec4::z,
                "w", &glm::vec4::w,
                
                sol::meta_function::addition, [](const glm::vec4& a, const glm::vec4& b) { return a + b; },
                sol::meta_function::addition, [](const glm::vec4& a, float b) { return a + b; },
                sol::meta_function::subtraction, [](const glm::vec4& a, const glm::vec4& b) { return a - b; },
                sol::meta_function::subtraction, [](const glm::vec4& a, float b) { return a - b; },
                sol::meta_function::multiplication, [](const glm::vec4& a, const glm::vec4& b) { return a * b; },
                sol::meta_function::multiplication, [](const glm::vec4& a, float b) { return a * b; },
                sol::meta_function::division, [](const glm::vec4& a, const glm::vec4& b) { return a / b; },
                sol::meta_function::division, [](const glm::vec4& a, float b) { return a / b; },
                sol::meta_function::unary_minus, [](const glm::vec4& a) { return -a; },
                sol::meta_function::equal_to, [](const glm::vec4& a, const glm::vec4& b) { return a == b; },

                "Lerp", [](const glm::vec4& a, const glm::vec4& b, float t) { return glm::mix(a, b, t); },
                "White", sol::var(glm::vec4(1.0f)),
                "Black", sol::var(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)),
                "Red", sol::var(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)),
                "Green", sol::var(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)),
                "Blue", sol::var(glm::vec4(0.0f, 0.0f, 1.0f, 1.0f))
            );

            lua.new_usertype<glm::vec3>("Vec3",
                sol::call_constructor, sol::constructors<glm::vec3(float, float, float), glm::vec3(float), glm::vec3()>(),

                "x", &glm::vec3::x,
                "y", &glm::vec3::y,
                "z", &glm::vec3::z,
                
                sol::meta_function::addition, [](const glm::vec3& a, const glm::vec3& b) { return a + b; },
                sol::meta_function::addition, [](const glm::vec3& a, float b) { return a + b; },
                sol::meta_function::subtraction, [](const glm::vec3& a, const glm::vec3& b) { return a - b; },
                sol::meta_function::subtraction, [](const glm::vec3& a, float b) { return a - b; },
                sol::meta_function::multiplication, [](const glm::vec3& a, const glm::vec3& b) { return a * b; },
                sol::meta_function::multiplication, [](const glm::vec3& a, float b) { return a * b; },
                sol::meta_function::division, [](const glm::vec3& a, const glm::vec3& b) { return a / b; },
                sol::meta_function::division, [](const glm::vec3& a, float b) { return a / b; },
                sol::meta_function::unary_minus, [](const glm::vec3& a) { return -a; },
                sol::meta_function::equal_to, [](const glm::vec3& a, const glm::vec3& b) { return a == b; },

                "Length", [](const glm::vec3& v) { return glm::length(v); },
                "Normalize", [](const glm::vec3& v) { return glm::normalize(v); },
                "Distance", [](const glm::vec3& v) { return glm::distance(v, v); },
                "Dot", [](const glm::vec3& a, const glm::vec3& b) { return glm::dot(a, b); },
                "Cross", [](const glm::vec3& a, const glm::vec3& b) { return glm::cross(a, b); },
                "Lerp", [](const glm::vec3& a, const glm::vec3& b, float t) { return glm::mix(a, b, t); }
            );

            lua.new_usertype<glm::vec2>("Vec2",
                sol::call_constructor, sol::constructors<glm::vec2(float, float), glm::vec2(float), glm::vec2()>(),

                "x", &glm::vec2::x,
                "y", &glm::vec2::y,
                
                sol::meta_function::addition, [](const glm::vec2& a, const glm::vec2& b) { return a + b; },
                sol::meta_function::addition, [](const glm::vec2& a, float b) { return a + b; },
                sol::meta_function::subtraction, [](const glm::vec2& a, const glm::vec2& b) { return a - b; },
                sol::meta_function::subtraction, [](const glm::vec2& a, float b) { return a - b; },
                sol::meta_function::multiplication, [](const glm::vec2& a, const glm::vec2& b) { return a * b; },
                sol::meta_function::multiplication, [](const glm::vec2& a, float b) { return a * b; },
                sol::meta_function::division, [](const glm::vec2& a, const glm::vec2& b) { return a / b; },
                sol::meta_function::division, [](const glm::vec2& a, float b) { return a / b; },
                sol::meta_function::unary_minus, [](const glm::vec2& a) { return -a; },
                sol::meta_function::equal_to, [](const glm::vec2& a, const glm::vec2& b) { return a == b; },

                "Length", [](const glm::vec2& v) { return glm::length(v); },
                "Normalize", [](const glm::vec2& v) { return glm::normalize(v); },
                "Distance", [](const glm::vec2& v) { return glm::distance(v, v); },
                "Dot", [](const glm::vec2& a, const glm::vec2& b) { return glm::dot(a, b); },
                "Lerp", [](const glm::vec2& a, const glm::vec2& b, float t) { return glm::mix(a, b, t); }
            );

            lua.new_usertype<glm::quat>("Quat",
                sol::call_constructor, sol::constructors<glm::quat(), glm::quat(const glm::vec3&), glm::quat(float, float, float, float)>(),

                "w", &glm::quat::w,
                "x", &glm::quat::x,
                "y", &glm::quat::y,
                "z", &glm::quat::z,
                
                sol::meta_function::multiplication, [](const glm::quat& a, const glm::quat& b) { return a * b; },
                sol::meta_function::multiplication, [](const glm::quat& q, const glm::vec3& v) { return q * v; },

                "ToEuler", [](const glm::quat& q) { return glm::eulerAngles(q); },
                "Conjugate", [](const glm::quat& q) { return glm::conjugate(q); },
                "Inverse", [](const glm::quat& q) { return glm::inverse(q); },
                "Normalize", [](const glm::quat& q) { return glm::normalize(q); },
                "Slerp", [](const glm::quat& a, const glm::quat& b, float t) { return glm::slerp(a, b, t); },
                "LookAt", [](const glm::vec3& direction, const glm::vec3& up){ return glm::quatLookAt(direction, up); }
            );

            auto mathTable = lua["Math"].get_or_create<sol::table>();
            mathTable.set_function("Clamp", [](float value, float min, float max) { return glm::clamp(value, min, max); });
            mathTable.set_function("Lerp", [](float a, float b, float t) { return glm::mix(a, b, t); });
            mathTable.set_function("Deg2Rad", [](float deg) { return glm::radians(deg); });
            mathTable.set_function("Rad2Deg", [](float rad) { return glm::degrees(rad); });
            mathTable.set_function("Damp", [](float a, float b, float lambda, float dt)
            {
               return glm::mix(a, b, 1.0f - glm::exp(-lambda * dt)); 
            });
            mathTable.set_function("RandomRange", [](float min, float max)
            {
                // TODO: Simple implementation using std::rand (replace with better engine RNG if we have one)
                float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                return min + r * (max - min);
            });
            mathTable.set_function("RandomInsideUnitSphere", []()
            {
                glm::vec3 p;
                do {
                    p = glm::vec3(
                        (float)rand()/RAND_MAX * 2.0f - 1.0f,
                        (float)rand()/RAND_MAX * 2.0f - 1.0f,
                        (float)rand()/RAND_MAX * 2.0f - 1.0f
                    );
                } while (glm::length2(p) >= 1.0f);
                return p;
            });
            mathTable.set_function("RandomUnitVector", []()
            {
                glm::vec3 p;
                do {
                    p = glm::vec3(
                        (float)rand()/RAND_MAX * 2.0f - 1.0f,
                        (float)rand()/RAND_MAX * 2.0f - 1.0f,
                        (float)rand()/RAND_MAX * 2.0f - 1.0f
                    );
                } while (glm::length2(p) >= 1.0f);
                return glm::normalize(p);
            });
        }
    }
}
