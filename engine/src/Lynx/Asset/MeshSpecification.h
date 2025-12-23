#pragma once

namespace Lynx
{
    struct StaticMeshSpecification
    {
        // Import Options
        bool CalculateNormals = false;
        bool CalculateTangents = false;
        bool FlipUVs = false;
        float GlobalScale = 1.0f;

        // Runtime Settings
        bool KeepCPUData = false; // Do we keep vertices in RAM after upload? (For physics/picking)

        std::string DebugName = "Mesh";
    };
}