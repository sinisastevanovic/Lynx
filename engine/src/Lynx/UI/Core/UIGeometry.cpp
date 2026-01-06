#include "UIGeometry.h"

namespace Lynx
{
    // Points are pinned to the same spot (Min == Max)
    const UIAnchor UIAnchor::Center      = { 0.5f, 0.5f, 0.5f, 0.5f };
    const UIAnchor UIAnchor::TopLeft     = { 0.0f, 0.0f, 0.0f, 0.0f };
    const UIAnchor UIAnchor::TopRight    = { 1.0f, 0.0f, 1.0f, 0.0f };
    const UIAnchor UIAnchor::BottomLeft  = { 0.0f, 1.0f, 0.0f, 1.0f };
    const UIAnchor UIAnchor::BottomRight = { 1.0f, 1.0f, 1.0f, 1.0f };

    // Stretch covers the whole range (0 to 1)
    const UIAnchor UIAnchor::StretchAll  = { 0.0f, 0.0f, 1.0f, 1.0f };
}
