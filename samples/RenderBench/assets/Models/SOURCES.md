# Where the models come from

The models of the Dense benchmark, with the triangles the engine loads from each.

| Folder | Source | Licence | Triangles |
| --- | --- | --- | --- |
| `coast_rocks_05` | https://polyhaven.com/a/coast_rocks_05 | CC0 | 771,724 |
| `root_cluster_01` | https://polyhaven.com/a/root_cluster_01 | CC0 | 225,261 |
| `potted_plant_01` | https://polyhaven.com/a/potted_plant_01 | CC0 | 176,226 |
| `shrub_01` | https://polyhaven.com/a/shrub_01 | CC0 | 156,012 |
| `dutch_ship_large_01` | https://polyhaven.com/a/dutch_ship_large_01 | CC0 | 110,616 |
| `FlightHelmet` | https://github.com/KhronosGroup/glTF-Sample-Assets | CC0, see its `LICENSE.md` | 94,722 |

The Poly Haven models are the glTF downloads with 2K textures.

Changed here: the glTF downloads mark leaves and sails as cut-out but come without the alpha map,
which Poly Haven offers as a separate texture. For `potted_plant_01` (leaves), `shrub_01` and
`dutch_ship_large_01` (sails) that map is stored in the alpha of the colour texture, which is a PNG instead of a JPEG for it.
