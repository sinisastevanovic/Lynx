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

The models for the forest of Renderer V2's stage 3. Each is one download that holds several objects,
and one `.gltf` per object here, see below.

| Folder | Source | Licence | Objects | Triangles per object |
| --- | --- | --- | --- | --- |
| `fir_sapling_medium` | https://polyhaven.com/a/fir_sapling_medium | CC0 | 3 trees | 420,822 to 685,046 |
| `fir_tree_01` | https://polyhaven.com/a/fir_tree_01 | CC0 | 3 trees | 505,494 to 4,176,819 |
| `pine_tree_01` | https://polyhaven.com/a/pine_tree_01 | CC0 | 3 trees | 4,234,432 to 6,951,524 |
| `grass_medium_01` | https://polyhaven.com/a/grass_medium_01 | CC0 | 17 clumps | 28 to 6,422 |

The Poly Haven models are the glTF downloads with 2K textures.

The four above are not in the repository, they are 1.5 GB. `tools/fetch-polyhaven.ps1` fetches and prepares
them, and a benchmark that uses them needs it run once. `polyhaven-files.json` holds the hash of every file
it made, so the script notices if a model comes out differently one day. Changed by it: a download holds
its objects side by side in one file, which the engine would load as one mesh. So there is one `.gltf` per
object, moved to the origin, and no `.gltf` of the whole download. They share the download's `.bin` and
textures. The script also stores the alpha of twigs and grass as described below and sets those
materials to cut-out, which the downloads mark as blended.

Changed here: the glTF downloads mark leaves and sails as cut-out but come without the alpha map,
which Poly Haven offers as a separate texture. For `potted_plant_01` (leaves), `shrub_01` and
`dutch_ship_large_01` (sails) that map is stored in the alpha of the colour texture, which is a PNG instead of a JPEG for it.
