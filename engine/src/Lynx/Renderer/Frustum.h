#pragma once
#include <glm/glm.hpp>
#include <array>

namespace Lynx
{
    struct AABB
    {
        glm::vec3 Min = glm::vec3(FLT_MAX);
        glm::vec3 Max = glm::vec3(-FLT_MAX);

        AABB() = default;
        AABB(const glm::vec3& min, const glm::vec3& max)
            : Min(min), Max(max) {}

        void Expand(const glm::vec3& point)
        {
            Min = glm::min(Min, point);
            Max = glm::max(Max, point);
        }

        std::array<glm::vec3, 8> GetCorners() const
        {
            std::array<glm::vec3, 8> corners;
            corners[0] = { Min.x, Min.y, Min.z };
            corners[1] = { Max.x, Min.y, Min.z };
            corners[2] = { Min.x, Max.y, Min.z };
            corners[3] = { Max.x, Max.y, Min.z };
            corners[4] = { Min.x, Min.y, Max.z };
            corners[5] = { Max.x, Min.y, Max.z };
            corners[6] = { Min.x, Max.y, Max.z };
            corners[7] = { Max.x, Max.y, Max.z };
            return corners;
        }
    };

    struct Frustum
    {
        std::array<glm::vec4, 6> Planes;

        void FromViewProjection(const glm::mat4& viewProj)
        {
            // Extract planes from VP matrix (Gribb/Hartmann method
            // Left
            Planes[0].x = viewProj[0][3] + viewProj[0][0];
            Planes[0].y = viewProj[1][3] + viewProj[1][0];
            Planes[0].z = viewProj[2][3] + viewProj[2][0];
            Planes[0].w = viewProj[3][3] + viewProj[3][0];

            // Right
            Planes[1].x = viewProj[0][3] - viewProj[0][0];
            Planes[1].y = viewProj[1][3] - viewProj[1][0];
            Planes[1].z = viewProj[2][3] - viewProj[2][0];
            Planes[1].w = viewProj[3][3] - viewProj[3][0];

            // Bottom
            Planes[2].x = viewProj[0][3] + viewProj[0][1];
            Planes[2].y = viewProj[1][3] + viewProj[1][1];
            Planes[2].z = viewProj[2][3] + viewProj[2][1];
            Planes[2].w = viewProj[3][3] + viewProj[3][1];

            // Top
            Planes[3].x = viewProj[0][3] - viewProj[0][1];
            Planes[3].y = viewProj[1][3] - viewProj[1][1];
            Planes[3].z = viewProj[2][3] - viewProj[2][1];
            Planes[3].w = viewProj[3][3] - viewProj[3][1];

            // Near
            Planes[4].x = viewProj[0][3] + viewProj[0][2];
            Planes[4].y = viewProj[1][3] + viewProj[1][2];
            Planes[4].z = viewProj[2][3] + viewProj[2][2];
            Planes[4].w = viewProj[3][3] + viewProj[3][2];

            // Far
            Planes[5].x = viewProj[0][3] - viewProj[0][2];
            Planes[5].y = viewProj[1][3] - viewProj[1][2];
            Planes[5].z = viewProj[2][3] - viewProj[2][2];
            Planes[5].w = viewProj[3][3] - viewProj[3][2];

            // Normalize planes
            for (auto& plane : Planes)
            {
                float length = glm::length(glm::vec3(plane));
                plane /= length;
            }
        }

        bool IsOnFrustum(const AABB& aabb) const
        {
            // Check if AABB is strictly BEHIND any plane
            glm::vec3 center = (aabb.Min + aabb.Max) * 0.5f;
            glm::vec3 extents = (aabb.Max - aabb.Min) * 0.5f;

            for (const auto& plane : Planes)
            {
                // Compute the projection interval radius of b onto L(t) = b.c + t * p.n
                float r = extents.x * std::abs(plane.x) +
                          extents.y * std::abs(plane.y) +
                          extents.z * std::abs(plane.z);

                float d = glm::dot(glm::vec3(plane), center);

                if (d + r < -plane.w) // Box is fully behind plane
                    return false;
            }
            return true;
        }
    };

    inline AABB TransformAABB(const AABB& localBounds, const glm::mat4& transform)
    {
        //AABB worldBounds;
        // TODO:
        // Optimization: Transform center and extent directly instead of 8 corners
        // (This is the standard efficient method)

        /*glm::vec3 min = localBounds.Min;
        glm::vec3 max = localBounds.Max;

        glm::vec3 xa = glm::vec3(transform[0] * min.x);
        glm::vec3 xb = glm::vec3(transform[0] * max.x);
        glm::vec3 ya = glm::vec3(transform[1] * min.y);
        glm::vec3 yb = glm::vec3(transform[1] * max.y);
        glm::vec3 za = glm::vec3(transform[2] * min.z);
        glm::vec3 zb = glm::vec3(transform[2] * max.z);

        glm::vec3 worldMin = glm::min(xa, xb) + glm::min(ya, yb) + glm::min(za, zb) + glm::vec3(transform[3]);
        glm::vec3 worldMax = glm::max(xa, xb) + glm::max(ya, yb) + glm::max(za, zb) + glm::vec3(transform[3]);

        return AABB(worldMin, worldMax);*/

        // 1. Get Local Center and Extents
        glm::vec3 center = (localBounds.Max + localBounds.Min) * 0.5f;
        glm::vec3 extent = (localBounds.Max - localBounds.Min) * 0.5f;

        // 2. Transform the Center
        // (Just a standard matrix * point)
        glm::vec3 worldCenter = glm::vec3(transform * glm::vec4(center, 1.0f));

        // 3. Transform the Extents
        // We take the absolute value of the rotation matrix columns to handle
        // the fact that AABBs don't rotate, they just expand.
        // This is the "Separating Axis Theorem" application for AABBs.

        glm::vec3 newExtent;

        // X Axis
        newExtent.x = std::abs(transform[0][0]) * extent.x +
                      std::abs(transform[1][0]) * extent.y +
                      std::abs(transform[2][0]) * extent.z;

        // Y Axis
        newExtent.y = std::abs(transform[0][1]) * extent.x +
                      std::abs(transform[1][1]) * extent.y +
                      std::abs(transform[2][1]) * extent.z;

        // Z Axis
        newExtent.z = std::abs(transform[0][2]) * extent.x +
                      std::abs(transform[1][2]) * extent.y +
                      std::abs(transform[2][2]) * extent.z;

        // 4. Reconstruct World AABB
        return AABB(worldCenter - newExtent, worldCenter + newExtent);
    }
}
