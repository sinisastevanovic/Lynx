#include "MathHelpers.h"

#include "Lynx/Engine.h"

namespace Lynx
{
    glm::vec3 MathHelpers::WorldToScreen(const glm::vec3& worldPosition, const glm::mat4& viewProjection)
    {
        glm::vec4 clipSpace = viewProjection * glm::vec4(worldPosition, 1.0f);
        if (clipSpace.w <= 0.0f)
            return glm::vec3(-10000);
        
        glm::vec3 ndc = glm::vec3(clipSpace) / clipSpace.w;
        
        auto [width, height] = Engine::Get().GetRenderer().GetViewportSize();
        glm::vec2 screenPos;
        screenPos.x = (ndc.x + 1.0f) * 0.5f * width;
        screenPos.y = (1.0f - ndc.y) * 0.5f * height; // TODO: Maybe don't flip? ((ndc.y + 1.0f) * 0.5f * height)
        
        return glm::vec3(screenPos, ndc.z);
    }

    glm::vec3 MathHelpers::WorldToCanvas(const glm::vec3& worldPosition, const glm::mat4& viewProjection, UICanvas* canvas)
    {
        glm::vec3 screenPos = WorldToScreen(worldPosition, viewProjection);
        if (canvas)
        {
            float scale = canvas->GetScaleFactor();
            screenPos.x /= scale;
            screenPos.y /= scale;
        }
        
        return screenPos;
    }
}
