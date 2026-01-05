#pragma once

#include <glm/glm.hpp>

#include "Lynx/UUID.h"

namespace Lynx
{
    struct CanvasComponent
    {
        bool IsScreenSpace = true;

        bool ScaleWithScreenSize = true;
        glm::vec2 ReferenceResolution = { 1920.0f, 1080.0f };

        // Match Width or Height? (0 = Width, 1 = Height, 0.5 = Average)
        float MatchWidthOrHeight = 0.5f;
        bool DistanceScaling = false;

        CanvasComponent() = default;
        CanvasComponent(const CanvasComponent&) = default;
    };

    struct RectTransformComponent
    {
        glm::vec2 AnchorMin = { 0.5f, 0.5f };
        glm::vec2 AnchorMax = { 0.5f, 0.5f };

        glm::vec2 OffsetMin = { -50.0f, -50.0f };
        glm::vec2 OffsetMax = { 50.0f, 50.0f };

        glm::vec2 Pivot = { 0.5f, 0.5f };

        float Rotation = 0.0f;
        glm::vec2 Scale = { 1.0f, 1.0f };

        // Cache
        glm::vec2 ScreenPosition = { 0.0f, 0.0f };
        glm::vec2 ScreenSize = { 100.0f, 100.0f };

        RectTransformComponent() = default;
        RectTransformComponent(const RectTransformComponent&) = default;
    };

    enum class ImageType { Simple, Sliced, Tiled, Filled };

    struct SpriteComponent
    {
        AssetHandle Material = AssetHandle::Null();
        int Layer = 0; // Sorting Order. Higher draws on top.

        ImageType Type = ImageType::Simple;

        float FillAmount = 1.0f;
        glm::vec4 Border = { 0.0f, 0.0f, 0.0f, 0.0f };
        glm::vec2 TileScale = { 1.0f, 1.0f };

        SpriteComponent() = default;
        SpriteComponent(const SpriteComponent&) = default;
    };
}
