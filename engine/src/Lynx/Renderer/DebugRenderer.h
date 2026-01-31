#pragma once
#include <glm/glm.hpp>

namespace Lynx
{
    struct DebugLine
    {
        glm::vec3 Start;
        glm::vec3 End;
        glm::vec4 Color;
        float LifeTime = 0.0f;
    };
    
    class LX_API DebugRenderer
    {
    public:
        static void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), float duration = 0.0f);
        static void DrawBox(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), float duration = 0.0f);
        static void DrawBox(const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), float duration = 0.0f);
        static void DrawSphere(const glm::vec3& center, float radius, const glm::vec4& color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), float duration = 0.0f);
        static void DrawCapsule(const glm::vec3& center, float radius, float halfHeight, const glm::quat& rotation, const glm::vec4& color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), float duration = 0.0f);

    private:
        static void EndFrame(float deltaTime);
        static const std::vector<DebugLine>& GetLines();
        static void Clear();

    private:
        static std::vector<DebugLine> s_Lines;
        friend class DebugPass;
        friend class Engine;
    };
    
}


