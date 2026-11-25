#pragma once

namespace Lynx
{
    // How bright the picture of a scene is made, as a camera's exposure does: the light of the scene is
    // multiplied by one number before it becomes colours on the screen. An entity carries this as its
    // "Exposure" component, and the first enabled one counts. A scene without one has the defaults.
    //
    // Automatic, the number follows what is in view, as the eye does: looking into shade makes the
    // picture brighter after a moment, stepping into the sun darker. The renderer measures how bright
    // the picture is and moves the exposure towards the one that shows its middle as a middle grey.
    struct ExposureSettings
    {
        bool IsAutomatic = true;

        // The exposure while it is not automatic. At 1 the light of the scene is shown as it is.
        float Fixed = 1.0f;

        // Makes the automatic picture brighter or darker than a middle grey, in stops: +1 is twice
        // as bright, -1 half.
        float Compensation = 0.0f;

        // The automatic exposure stays between these two, so that a dark room is not shown as bright
        // as day, nor a look into the sun as grey.
        float Lowest = 0.1f;
        float Highest = 8.0f;

        // How fast the exposure follows, per second: towards darker when the view gets brighter, and
        // towards brighter when the view gets darker. The eye is quicker at the first.
        float SpeedTowardsDarker = 3.0f;
        float SpeedTowardsBrighter = 1.0f;

        bool operator==(const ExposureSettings&) const = default;
    };
}
