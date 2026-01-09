#pragma once

#include <glm/glm.hpp>

namespace Lynx
{
    class UICanvas;

    class LX_API MathHelpers
    {
    public:
        static glm::vec3 WorldToScreen(const glm::vec3& worldPosition, const glm::mat4& viewProjection);
        static glm::vec3 WorldToCanvas(const glm::vec3& worldPosition, const glm::mat4& viewProjection, UICanvas* canvas);
    };
}

