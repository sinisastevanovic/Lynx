#pragma once

namespace Lynx
{
    // The stretch of a camera's view that holds something: how far from the camera the nearest and the
    // farthest surface it sees are, measured along the way it looks. The sky is no surface.
    struct ViewDepthRange
    {
        float Nearest = 0.0f;
        float Farthest = 0.0f;
    };
}
