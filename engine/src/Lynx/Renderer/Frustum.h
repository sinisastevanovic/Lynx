#pragma once
#include <glm/glm.hpp>
#include <array>

namespace Lynx
{
    // A box along the axes around a set of points.
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
    };

    // The six planes around what a view sees, pointing inwards: the normal in xyz, the distance in w.
    // Meshes are culled with them on the GPU (Cull.slang), see-through ones here on the CPU.
    struct Frustum
    {
        std::array<glm::vec4, 6> Planes;

        // The planes are read straight from the rows of the matrix (Gribb and Hartmann): left, right,
        // bottom, top, near and far.
        void FromViewProjection(const glm::mat4& viewProj)
        {
            const auto row = [&viewProj](int index)
            {
                return glm::vec4(viewProj[0][index], viewProj[1][index], viewProj[2][index], viewProj[3][index]);
            };

            Planes[0] = row(3) + row(0);
            Planes[1] = row(3) - row(0);
            Planes[2] = row(3) + row(1);
            Planes[3] = row(3) - row(1);
            Planes[4] = row(3) + row(2);
            Planes[5] = row(3) - row(2);

            // With normals of length one, a plane gives the distance of a point from it.
            for (glm::vec4& plane : Planes)
                plane /= glm::length(glm::vec3(plane));
        }

        // False only if the box lies wholly behind one of the six planes: its corner farthest along a
        // plane's normal is tested, which is behind the plane only if all of the box is.
        bool IsOnFrustum(const AABB& box) const
        {
            for (const auto& plane : Planes)
            {
                const glm::vec3 farthest(plane.x >= 0.0f ? box.Max.x : box.Min.x, plane.y >= 0.0f ? box.Max.y : box.Min.y, plane.z >= 0.0f ? box.Max.z : box.Min.z);
                if (glm::dot(glm::vec3(plane), farthest) + plane.w < 0.0f)
                    return false;
            }
            return true;
        }

        // False only if the sphere lies wholly behind one of the six planes.
        bool IsOnFrustum(const glm::vec3& centre, float radius) const
        {
            for (const auto& plane : Planes)
            {
                if (glm::dot(glm::vec3(plane), centre) + plane.w < -radius)
                    return false;
            }
            return true;
        }
    };
}
