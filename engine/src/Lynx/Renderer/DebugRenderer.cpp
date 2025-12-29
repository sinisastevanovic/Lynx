#include "DebugRenderer.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
namespace Lynx
{
    std::vector<DebugLine> DebugRenderer::s_Lines;
    
    void DebugRenderer::DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color)
    {
        s_Lines.push_back(DebugLine(start, end, color));
    }

    void DebugRenderer::DrawBox(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color)
    {
        // 12 Lines for an AABB
        glm::vec3 p0 = { min.x, min.y, min.z };
        glm::vec3 p1 = { max.x, min.y, min.z };
        glm::vec3 p2 = { max.x, min.y, max.z };
        glm::vec3 p3 = { min.x, min.y, max.z };

        glm::vec3 p4 = { min.x, max.y, min.z };
        glm::vec3 p5 = { max.x, max.y, min.z };
        glm::vec3 p6 = { max.x, max.y, max.z };
        glm::vec3 p7 = { min.x, max.y, max.z };

        // Bottom
        DrawLine(p0, p1, color); DrawLine(p1, p2, color);
        DrawLine(p2, p3, color); DrawLine(p3, p0, color);

        // Top
        DrawLine(p4, p5, color); DrawLine(p5, p6, color);
        DrawLine(p6, p7, color); DrawLine(p7, p4, color);

        // Pillars
        DrawLine(p0, p4, color); DrawLine(p1, p5, color);
        DrawLine(p2, p6, color); DrawLine(p3, p7, color);
    }

    void DebugRenderer::DrawBox(const glm::mat4& transform, const glm::vec4& color)
    {
        // Unit Cube corners (-0.5 to 0.5) transformed
        glm::vec3 corners[8] = {
            { -0.5f, -0.5f, -0.5f }, {  0.5f, -0.5f, -0.5f },
            {  0.5f, -0.5f,  0.5f }, { -0.5f, -0.5f,  0.5f },
            { -0.5f,  0.5f, -0.5f }, {  0.5f,  0.5f, -0.5f },
            {  0.5f,  0.5f,  0.5f }, { -0.5f,  0.5f,  0.5f }
        };

        for (int i = 0; i < 8; i++)
            corners[i] = glm::vec3(transform * glm::vec4(corners[i], 1.0f));

        // Bottom
        DrawLine(corners[0], corners[1], color); DrawLine(corners[1], corners[2], color);
        DrawLine(corners[2], corners[3], color); DrawLine(corners[3], corners[0], color);
        // Top
        DrawLine(corners[4], corners[5], color); DrawLine(corners[5], corners[6], color);
        DrawLine(corners[6], corners[7], color); DrawLine(corners[7], corners[4], color);
        // Vertical
        DrawLine(corners[0], corners[4], color); DrawLine(corners[1], corners[5], color);
        DrawLine(corners[2], corners[6], color); DrawLine(corners[3], corners[7], color);
    }

    void DebugRenderer::DrawSphere(const glm::vec3& center, float radius, const glm::vec4& color)
    {
        // Draw 3 circles (XY, XZ, YZ planes)
        const int segments = 24;
        const float step = glm::two_pi<float>() / segments;

        for (int i = 0; i < segments; i++)
        {
            float theta1 = i * step;
            float theta2 = (i + 1) * step;

            float s1 = sin(theta1) * radius;
            float c1 = cos(theta1) * radius;
            float s2 = sin(theta2) * radius;
            float c2 = cos(theta2) * radius;

            // XY Plane
            DrawLine(center + glm::vec3(c1, s1, 0), center + glm::vec3(c2, s2, 0), color);
            // XZ Plane
            DrawLine(center + glm::vec3(c1, 0, s1), center + glm::vec3(c2, 0, s2), color);
            // YZ Plane
            DrawLine(center + glm::vec3(0, c1, s1), center + glm::vec3(0, c2, s2), color);
        }
    }

    void DebugRenderer::DrawCapsule(const glm::vec3& center, float radius, float halfHeight, const glm::quat& rotation, const glm::vec4& color)
    {
        const int segments = 16;
        const float step = glm::pi<float>() / segments;

        glm::vec3 up = rotation * glm::vec3(0, 1, 0);
        glm::vec3 right = rotation * glm::vec3(1, 0, 0);
        glm::vec3 forward = rotation * glm::vec3(0, 0, 1);

        glm::vec3 topCenter = center + up * halfHeight;
        glm::vec3 bottomCenter = center - up * halfHeight;

        // Draw 4 vertical lines (The Cylinder part)
        DrawLine(topCenter + right * radius, bottomCenter + right * radius, color);
        DrawLine(topCenter - right * radius, bottomCenter - right * radius, color);
        DrawLine(topCenter + forward * radius, bottomCenter + forward * radius, color);
        DrawLine(topCenter - forward * radius, bottomCenter - forward * radius, color);

        // Draw Top and Bottom Circles
        const float circleStep = glm::two_pi<float>() / segments;
        for (int i = 0; i < segments; i++)
        {
            float t1 = i * circleStep;
            float t2 = (i + 1) * circleStep;

            glm::vec3 p1 = right * cos(t1) * radius + forward * sin(t1) * radius;
            glm::vec3 p2 = right * cos(t2) * radius + forward * sin(t2) * radius;

            DrawLine(topCenter + p1, topCenter + p2, color);
            DrawLine(bottomCenter + p1, bottomCenter + p2, color);
        }

        // Draw Arcs (The Hemispheres)
        for (int i = 0; i < segments; i++)
        {
            float t1 = i * step;
            float t2 = (i + 1) * step;

            // XY arc
            DrawLine(topCenter + (right * cos(t1) + up * sin(t1)) * radius,
                     topCenter + (right * cos(t2) + up * sin(t2)) * radius, color);
            DrawLine(bottomCenter + (right * cos(t1) - up * sin(t1)) * radius,
                     bottomCenter + (right * cos(t2) - up * sin(t2)) * radius, color);

            // ZY arc
            DrawLine(topCenter + (forward * cos(t1) + up * sin(t1)) * radius,
                     topCenter + (forward * cos(t2) + up * sin(t2)) * radius, color);
            DrawLine(bottomCenter + (forward * cos(t1) - up * sin(t1)) * radius,
                     bottomCenter + (forward * cos(t2) - up * sin(t2)) * radius, color);
        }
    }

    const std::vector<DebugLine>& DebugRenderer::GetLines()
    {
        return s_Lines;
    }

    void DebugRenderer::Clear()
    {
        s_Lines.clear();
    }
}
