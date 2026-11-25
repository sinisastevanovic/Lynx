#pragma once

#include "Lynx/Asset/AssetRef.h"
#include "Lynx/Asset/Terrain.h"

namespace Lynx
{
    // Puts a terrain into the scene. The entity's position is where the terrain's first corner lies,
    // the one with the smallest x and z. Its rotation and scale do not count: a terrain's size is its own.
    struct TerrainComponent
    {
        AssetRef<Terrain> Terrain;
    };
}
