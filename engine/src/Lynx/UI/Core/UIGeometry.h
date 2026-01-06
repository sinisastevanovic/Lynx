#pragma once

#include <glm/glm.hpp>

namespace Lynx
{
    // Density-independant pixels (160 dpi = 1.0)
    using dp = float;

    struct UIPoint { dp X = 0.0f; dp Y = 0.0f;};
    struct UISize { dp Width = 0.0f; dp Height = 0.0f; };

    struct UIRect
    {
        dp X = 0.0f;
        dp Y = 0.0f;
        dp Width = 0.0f;
        dp Height = 0.0f;

        bool Contains(UIPoint p) const
        {
            return p.X >= X && p.X <= X + Width && p.Y >= Y && p.Y <= Y + Height;
        }
    };

    struct UIThickness
    {
        dp Left = 0.0f;
        dp Top = 0.0f;
        dp Right = 0.0f;
        dp Bottom = 0.0f;

        UIThickness() = default;
        UIThickness(dp uniform) : Left(uniform), Top(uniform), Right(uniform), Bottom(uniform) {}
        UIThickness(dp horizontal, dp vertical) : Left(horizontal), Top(vertical), Right(horizontal), Bottom(vertical) {}
        UIThickness(dp l, dp t, dp r, dp b) : Left(l), Top(t), Right(r), Bottom(b) {}
    };

    struct UIAnchor
    {
        float MinX = 0.5f;
        float MinY = 0.5f;
        float MaxX = 0.5f;
        float MaxY = 0.5f;

        static const UIAnchor Center;
        static const UIAnchor TopLeft;
        static const UIAnchor TopRight;
        static const UIAnchor BottomLeft;
        static const UIAnchor BottomRight;
        static const UIAnchor StretchAll;
    };
}
