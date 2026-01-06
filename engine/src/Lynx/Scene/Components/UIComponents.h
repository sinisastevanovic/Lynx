#pragma once

#include "Lynx/UI/Core/UICanvas.h"

namespace Lynx
{
    struct UICanvasComponent
    {
        std::shared_ptr<UICanvas> Canvas;

        UICanvasComponent()
        {
            Canvas = std::make_shared<UICanvas>();
        }

        UICanvasComponent(const UICanvasComponent& other)
        {
            // TODO: Deep copy the UI tree!
            // This shared_ptr copy is dangerous if we modify it
            Canvas = other.Canvas;
        }
    };
}
