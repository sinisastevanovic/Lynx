#include "ShadowCascades.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace Lynx
{
    std::array<ShadowCascade, ShadowCascadeCount> FitShadowCascades(const glm::mat4& view, const glm::mat4& projection,
        const glm::vec3& sunDirection, const ShadowSettings& settings)
    {
        // How wide the view is at a distance of one, in x and y. An orthographic view is as wide everywhere.
        const bool isPerspective = projection[2][3] != 0.0f;
        const float halfWidth = 1.0f / std::abs(projection[0][0]);
        const float halfHeight = 1.0f / std::abs(projection[1][1]);
        const float nearPlane = isPerspective ? std::max(projection[3][2] / projection[2][2], 0.01f) : 0.0f;
        const float farPlane = std::max(settings.Distance, nearPlane + 1.0f);

        const glm::mat4 cameraToWorld = glm::inverse(view);
        const glm::vec3 cameraPosition = glm::vec3(cameraToWorld[3]);
        const glm::vec3 cameraForward = -glm::normalize(glm::vec3(cameraToWorld[2]));

        // The sun's view of the world, turned but not moved: where a cascade lies in it is its own matter.
        const glm::vec3 direction = glm::normalize(sunDirection);
        const glm::vec3 up = std::abs(direction.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::mat4 sunView = glm::lookAt(glm::vec3(0.0f), direction, up);

        std::array<ShadowCascade, ShadowCascadeCount> cascades;
        float sliceNear = nearPlane;
        for (uint32_t index = 0; index < ShadowCascadeCount; ++index)
        {
            // Between sharing the distance out evenly and making every cascade as many times deeper than the last.
            const float share = static_cast<float>(index + 1) / static_cast<float>(ShadowCascadeCount);
            const float evenly = nearPlane + (farPlane - nearPlane) * share;
            const float byRatio = isPerspective ? nearPlane * std::pow(farPlane / nearPlane, share) : evenly;
            const float sliceFar = glm::mix(evenly, byRatio, settings.SplitWeight);

            // The smallest sphere around the stretch of the view from sliceNear to sliceFar. Its centre
            // lies on the line the camera looks along.
            float centreDistance;
            float radius;
            if (isPerspective)
            {
                const float widening = halfWidth * halfWidth + halfHeight * halfHeight;
                centreDistance = std::min(0.5f * (sliceNear + sliceFar) * (1.0f + widening), sliceFar);
                radius = std::sqrt((sliceFar - centreDistance) * (sliceFar - centreDistance) + sliceFar * sliceFar * widening);
            }
            else
            {
                const float halfDepth = 0.5f * (sliceFar - sliceNear);
                centreDistance = sliceNear + halfDepth;
                radius = std::sqrt(halfDepth * halfDepth + halfWidth * halfWidth + halfHeight * halfHeight);
            }

            // Rounded up, so that the last digits of the arithmetic above do not change the size of a
            // texel from frame to frame.
            radius = std::ceil(radius * 16.0f) / 16.0f;

            // The centre, in the sun's view, on a grid of whole texels.
            const float texelSize = 2.0f * radius / static_cast<float>(ShadowCascadeResolution);
            glm::vec3 centre = glm::vec3(sunView * glm::vec4(cameraPosition + cameraForward * centreDistance, 1.0f));
            centre.x = std::floor(centre.x / texelSize) * texelSize;
            centre.y = std::floor(centre.y / texelSize) * texelSize;

            // The sun looks along -z, so what stands between it and the sphere has a larger z.
            const float nearestToSun = -(centre.z + radius + settings.DepthRange);
            const float farthestFromSun = -(centre.z - radius - settings.DepthRange);
            const glm::mat4 sunProjection = glm::ortho(centre.x - radius, centre.x + radius, centre.y - radius, centre.y + radius, nearestToSun, farthestFromSun);

            cascades[index].ViewProjection = sunProjection * sunView;
            cascades[index].SunView = sunView;
            cascades[index].Centre = glm::vec2(centre);
            cascades[index].Radius = radius;
            cascades[index].TexelSize = texelSize;
            sliceNear = sliceFar;
        }

        return cascades;
    }
}
