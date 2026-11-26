# LynxEngine

A hobby 3D game engine in C++23: Vulkan 1.3 through a thin layer of its own, EnTT, Jolt physics, Lua through sol2, an ImGui editor.
One developer. The engine was reviewed in November 2026 and is being refactored step by step, not rewritten.

Current state, open problems and the order of work: https://claude.ai/artifact/8UigvboHdEzfD3iVqeeeyy
Read its "Status" section before starting anything larger. Its "Target design" section describes where the
structure is heading, and "Toward the target design" the order of the steps.

Renderer work follows its own plan, Renderer V2: https://claude.ai/artifact/SShPW5mVGsxRJYgqVdASKj
It is the main line of work. Read its "Decisions" and "Status" sections before touching the renderer, and tick off
the steps of the stage in work as they land. Stages 1 and 2 are complete. The non-renderer steps of the review are paused meanwhile.

## Repositories

- `D:\LynxEngine` (this one): engine and editor. Work happens on the `development` branch.
- `D:\LynxSandboxGame`: the game used to exercise the engine. It builds the engine as a subdirectory.

## Layout

- `engine/src/Lynx/` engine library, built as `engine.dll`
  - `Engine.cpp` startup, main loop, play lifecycle
  - `Core/` `ContentPaths` (all file locations), `WorkerPool`, `DllLoader`
  - `Project/` the `.lxproj` project file
  - `Reflection/` field lists for components
  - `Scene/` scene, entities, components, serializer. Engine components are registered in `Scene/Components/CoreComponentRegistration.cpp`
  - `Asset/` asset registry and manager
  - `Renderer/` renderer and `Passes/`. `Renderer/Vulkan/` is the engine's own thin layer over Vulkan
  - `Physics/`, `Scripting/`, `UI/` (the in-game UI), `ImGui/` (editor widgets, `LXUI`)
- `engine/resources/` shaders, default textures, the default editor layout
- `engine/launch/` sources compiled into every game: module exports and the standalone `main`
- `editor/src/` the editor: `EditorLayer`, `Panels/`, `Inspectors/`
- `cmake/LynxGame.cmake` `lynx_add_game()`, which builds a game
- `samples/RenderBench/` the project the renderer is measured with: benchmark scenes, and one file per benchmark in `bench/`.
  Its code hands out asset IDs of its own, meshes from 7300000000000000000, materials from 7400000000000000000, and a terrain with its layers from 7600000000000000001:
  an asset file in that project must not use one of those.
- `tools/` scripts for verifying changes

## Build and run

Day to day, build from the game project. Its solution contains the engine and the editor.

```
cd D:\LynxSandboxGame
cmake --preset default            # only after adding or removing files, or changing CMake
cmake --build --preset debug
build\bin\Debug\editor.exe        # run from the project folder, or pass the .lxproj path
build\bin\Debug\LynxSandboxGame.exe
```

- vcpkg provides the libraries. Its version is pinned by `builtin-baseline` in `vcpkg.json`. `VCPKG_ROOT` must be set.
- The log is written to `<project>\temp\engine.log`. The shader cache, the mesh cache and editor layout are in the same folder.
- Only Debug and Release exist. Release has not been tested recently.

## How things work

- **Paths.** Nothing depends on the working directory. Ask `ContentPaths` for a location, and use
  `AssetManager::GetEngineAsset` or `GetProjectAsset` for assets.
- **Components.** Register a component with a field list (`Reflection/Field.h`). Saving, loading, the inspector and
  Lua access come from it. The first argument of `fields.Add` is the key in saved files: never rename it without
  migrating the scenes. Only components with real custom logic keep hand-written save and load code.
- **Change notification.** Nothing scans the scene for changes. Position, rotation and scale of an entity, its
  mesh and its materials are private and written through setters on the component: `transform.SetTranslation(v)`,
  `SetRotation`, `SetScale`, `SetTransform`, `mesh.SetMesh(handle)`, `mesh.SetMaterial(index, handle)`.
  `entity.SetTranslation(v)` is short for the first. A write that tells nobody does not compile. The two components
  know the scene and the entity they belong to, which the scene sets when they are added. The scene keeps a list
  of written transforms and works out world matrices for those only (`UpdateGlobalTransforms`). The renderer
  listens for EnTT's `on_update<MeshComponent>`. Code that sets a field through the field system (loading, the
  generic inspector, Lua) calls `ComponentInfo::notifyChanged` afterwards, which is `registry.patch<T>(entity)`.
  A component with private members describes its own fields in a static `DescribeFields`.
- **Inspectors.** Component UI code belongs in the editor (`editor/src/Inspectors`), not in the engine.
- **Play.** The engine starts in Edit state. `StartPlay` and `StopPlay` are the only way in and out. A copy of the
  scene is played, never the loaded asset itself.
- **Assets.** Reading a file may happen on any thread. Creating GPU resources happens on the main thread only.
- **Shutdown order matters.** Scenes go before the script engine, assets before the renderer. See `Engine::Shutdown`.
- **Shaders.** Shaders are written in Slang (`.slang`) and compiled once into `temp\ShaderCache`. Mark entry points
  with `[shader("vertex")]`. Shared code lives in modules (`Scene`, `Textures`, `Mesh`, `Material`, `Atmosphere`, `StandardShading`) that a
  shader imports. There is no GLSL any more.
- **Passes.** Every pass of a frame is a `VulkanPass` in the render graph (`Renderer/Graph`). A pass declares what it
  reads and draws into, and records draws. The graph does layouts, begins rendering and binds the shared set.
- **Compute.** A pass that sets `PassResources::Computes` draws nothing and dispatches compute shaders
  (`Vulkan/ComputePipeline`). It reaches its buffers through their addresses in push constants, like every shader.
  The graph binds the shared set for compute and, after the pass, makes later passes wait for what it wrote.
- **Shared bindings.** Passes bind nothing themselves. One descriptor set (`Vulkan/SharedBindings`) holds the scene
  constants and tables of all textures and samplers. A shader gets texture indices, buffer addresses and everything
  else through push constants (128 bytes at most). Slang pads push constants to a multiple of their largest member:
  the struct in C++ needs the same padding, and `SharedBindings::Supports` reports both sizes if they differ.
- **Lifetimes.** A frame is still on the GPU while the next ones are recorded. `GpuBuffer` and `GpuTexture` are
  released a few frames after they are given up, so nothing has to wait for the GPU. `TextureSlot` does the same
  for a texture's place in the texture table.
- **Layouts.** A `GpuTexture` remembers the layout it is in. `Transition` changes it. The render graph does that
  for everything a pass declares.
- **Materials.** All drawn materials are in one buffer on the GPU (`Renderer/MaterialTable`). An instance carries the
  index of its material and the shader reads the material from the table. A mesh pass sets its push constants once,
  and a draw passes only where its instances start (`firstInstance`, read in shaders as `SV_VulkanInstanceID`).
  Textures picked through the table need `NonUniformResourceIndex`.
- **Material overrides.** A mesh comes with a material per submesh, made from its file. A mesh component can name
  material assets (`.lxmat`) that take their place for that entity (`MeshComponent::Materials`, one per submesh, an
  empty reference keeps the mesh's own). So many entities share one mesh and its geometry and still look different.
  The list is saved through the field system as an `AssetList` field.
- **Geometry.** The vertices and indices of all meshes are in one pair of buffers (`Vulkan/GeometryPool`). A submesh
  is a range in them (`MeshGeometry`). A pass binds the pool once and draws ranges. The buffers grow when they fill up.
- **Cooked meshes.** A model file is cooked once (`Asset/MeshCooking`): triangles and vertices are put into the order
  the GPU draws fastest, with meshoptimizer, and vertices are packed from 64 into 28 bytes (`Renderer/MeshVertex.h`).
  The result is kept in `temp\MeshCache` and used until the model file or its `.bin` changes. A change to cooking or
  to `PackedVertex` needs the version in `MeshCache.cpp` bumped. Texture coordinates stay exact on purpose.
- **Scene on the GPU.** Every submesh of every mesh in the scene is an instance in one buffer that stays on the GPU
  (`Renderer/GpuScene`). `SceneRenderer` keeps it in step with the scene and passes on only what changed: mesh
  entities that came, went or got another mesh or material (EnTT signals), entities that moved
  (`Scene::GetMovedEntities`), meshes that were reloaded. What it keeps per entity is a component of its own in the scene's registry (`MeshInstances`), which is
  never saved. A frame draws a list of places in the instance buffer, which culling writes.
  Where an instance is in the world is not part of it: transforms are 3 by 4 matrices in two buffers of their
  own (`InstanceTransform`, `MeshTransform` in `Mesh.slang`), one for this frame and one for the last, which
  change roles every frame. A moved instance costs one transform of 48 bytes a frame, and a shader builds
  the matrix with `meshModel`.
- **Motion.** Next to the colour, the scene passes write how far every pixel moved on screen since the last
  frame, in texture coordinates, where it is minus where it was (`RenderData::SceneMotion`, two half floats,
  `screenMotion` in `Scene.slang`). Meshes take it from last frame's transform and last frame's camera
  (`scene.previousViewProjection`), terrain and sky from the camera alone. A pass that writes it sets
  `PassResources::MotionTarget` and its pipeline `MotionFormat`. A fragment shader's outputs are numbered
  colour, motion, entity IDs. A pipeline that blends leaves the motion alone. Nothing reads it yet but the
  view mode "Motion", until TAA (step 3.10). What stands still has a motion of exactly zero.
- **Jitter.** With `TemporalSettings::Jitters` (`Renderer::GetTemporalSettings`, off by default) the camera is
  shifted by a part of a pixel, differently in every frame, along the Halton sequence of bases 2 and 3, by a
  count of frames so that runs repeat. `scene.viewProjection` holds the shift, so everything drawn with it is
  shifted. `scene.unjitteredViewProjection` and `scene.previousViewProjection` do not, and motion is worked
  out with those two alone. `scene.jitter` is the shift in texture coordinates.
- **Pictures written by compute.** A compute shader writes a picture pixel by pixel through a table of its
  own, `writableTextures[index][pixel] = colour` in `Textures.slang`. The texture is made with the storage
  usage and added with `SharedBindings::AddWritableTexture` (a `TextureSlot` with `TextureTable::Writable`),
  and the pass names it with `PassResources::Write`, which brings it into the general layout. To be read
  afterwards it is in the table of textures as well.
- **Temporal pass.** `Passes/TemporalPass` (`Temporal.slang`) runs after everything of the scene is drawn and
  before exposure, bloom and the composite pass, which then work with its result (it points
  `RenderData::SceneColor` at its own picture). It is where anti-aliasing over time is being built, see step
  3.10 of Renderer V2 and `docs/High-Quality TAA Implementation Brief.md`.
  With `TemporalSettings::Resolves` (off by default, "Temporal anti-aliasing" in the render settings panel) it
  is the anti-aliasing: a pixel takes the motion of the nearest thing around it, reads the history there with
  the Catmull-Rom curve, pulls it into the box of the colours around the pixel (in YCoCg, rounded as Karis
  does, `BoxGamma` for variance clipping), and mixes it with the frame, weighted by brightness after exposure.
  The history's alpha counts the frames it is made of, and `HistoryTime` sets the least a frame counts.
  The history is dropped off screen and where the depths around the pixel now and around its place then
  have nothing in common: single depths cannot be compared, the jitter moves every edge. The pass keeps
  last frame's depth itself. Two things keep it sharp in motion. A frame counts for more where the history
  is read between pixels (`BetweenPixelsShare`), because that read blurs and the blur adds up. And colour
  textures are read where the pixel would be without the jitter (`SteadyTextures`, `steadyUv` in
  `Scene.slang`), so their patterns stand still: a new shader that reads a colour texture should do the
  same, and read what decides a cut-out with the jittered coordinates. Terrain does not do it yet.
  A pixel that stands still, with the same surface behind it as before, gets a wider box and a longer
  history (`StillBoxScale`, `StillHistoryTime`), or what is thinner than a pixel flickers at rest. Both
  fade out over half a pixel of motion a frame.
  The view modes "TAA: ..." show the share of the frame, where the history was dropped and how far it was
  pulled in: the pass draws them into a picture that the composite pass shows in place of the scene
  (`RenderData::ShownInstead`). The grid and debug lines are drawn after it, without the jitter.
  With `TemporalSettings::Accumulates` it averages the frames of a view that stands still instead: with
  jitter, the picture with as many samples a pixel as the jitter is long, which the resolve is measured
  against. FXAA is off while either is on.
  `Renderer::ResetHistory` says that the next frame shows something else than the last (another scene, a
  camera that jumped): call it, or what is put together from several frames smears across the change.
- **Mesh draws.** Meshes that are not see-through are drawn from a buffer of draw commands, one per submesh with all
  its instances that are seen (`RenderData::MeshDraws`). A pass draws a whole range of them with one call
  (`DrawMeshes`). Depth and shadow use a pipeline without a fragment shader for opaque materials and a second one
  for cut-outs. See-through meshes are culled and sorted on the CPU and drawn one by one.
  The commands of a view are grouped by kind (`MeshDrawKind`: cut-outs or not, two-sided or not) and a pass walks
  through the groups. A further way of drawing is a further member of the kind.
- **Two-sided materials.** `Material::TwoSided`, read from glTF's `doubleSided`. The mesh passes set which side is
  left out while drawing (`SetCulling`, a dynamic state), so no pipeline exists twice for it. Blender exports every
  material as two-sided unless backface culling is ticked: on closed surfaces that costs 20 to 30 % of the frame.
- **Culling.** The GPU decides what is drawn (`Passes/CullPass`, `Cull.slang`). The CPU lays out one draw command
  per batch (the instances of one submesh in one material) and view, drawing nothing, each with a run of the list of drawn
  instances long enough for the whole batch. The compute shader tests the sphere of every instance against the
  camera and the sun, counts those seen into the command and writes their places into its run. A command may end
  up drawing nothing, and the order of instances within a batch differs from frame to frame. The CPU learns what
  was drawn four frames later, from a copy of the commands, and only for the counters.
- **Levels of detail.** Cooking makes coarser levels of every submesh of 1,024 triangles or more, each about half
  the one before, with meshoptimizer (`BuildLods`). A level is a further run of indices over the same vertices
  (`MeshLod`, `Submesh::Lods`) and knows how far it is off the original. The cull shader picks, per instance, the
  coarsest level whose error stays under `LodSettings::ErrorPixels` on screen, so a batch has a draw command per
  level and view. Shadows are drawn one level coarser. `LodSettings::ForcedLevel` shows one level everywhere
  ("Show level" in the renderer settings panel).
- **Foliage levels.** Simplifying leaves and needles makes trees visibly barer, so foliage gets its levels another
  way (`BuildFoliageLods`, after Pixar's stochastic pruning). Foliage is a submesh with cut-outs made of at least
  256 separate pieces. The cooker stores those "leaves" in a random order, so a level is the first part of the
  index list and costs nothing, and gives every vertex the centre of its leaf (`LeafVertex`, 8 bytes, in a buffer
  per submesh that an instance carries the address of). A coarser level draws half the leaves, and the vertex
  shaders grow the rest about their centres so the crown covers the same area (`leafPosition` in `Mesh.slang`).
  The level of foliage is picked in sixteenths and a leaf shrinks away before it is dropped, so nothing pops.
  A piece more than four times the usual leaf size, like a twig's stem, is kept until the leaves have
  grown to its size, and thinned out like them from there on. A piece is not grown as a whole: every vertex
  knows the level from which it moves outwards, which is when the usual leaf has grown out to where the vertex
  already is. Grown about its middle, a piece longer than a leaf stuck out of the crown as a spike.
  `LodSettings::LeafPixels` says
  how large a leaf may be on screen. An entry of the list of drawn instances holds the place in 24 bits and the
  level above them. A surface with cut-outs of few pieces, like a sail, keeps its one level.
- **View modes.** A view of a scene can show one thing about the picture instead of the picture (`ViewMode` in
  `Renderer/ViewMode.h`: wireframe, unlit, lighting only, normals, level of detail, shadow cascades, motion, three of the TAA) and draw cut-outs whole
  (`ViewSettings::ShowCutOuts`). The settings belong to the view and travel with it (`ExtractedScene::Viewing`),
  so two views can show a scene differently. `SceneRenderer::GetViewSettings` holds those of the main view, the
  viewport panel has the dropdown, and a benchmark takes `ViewMode=<number>` and `ShowCutOuts=0`
  (and `SunElevation=<degrees>` for a low sun). The forward
  shader does the modes, but the motion, which the composite pass shows from the motion picture. Modes with raw colours skip bloom and tone mapping (`ShowsRawColours`). A new mode is a
  new enum value, its name, and a branch in `StandardShading.slang`.
- **Shadows.** The sun's shadow is in four maps of 2048 texels, the cascades (`Renderer/ShadowCascades`), each
  for a stretch of what the camera sees. `FitShadowCascades` fits each to the sphere around its stretch and moves
  it in whole texels, so shadows do not crawl. One `ShadowPass` per cascade ("Shadow0" to "Shadow3"). A shader
  takes a place's shadow from the first cascade whose map it lies in and mixes it with the next near the map's
  edge (`sunVisibility` in `StandardShading.slang`). Culling leaves an instance out of a cascade if the cascade
  before shows all of its shadow, or if it is smaller than `ShadowSettings::SmallestCasterInTexels` of the map,
  and draws it no finer than the map can show. Without those three, shadows cost 10 to 80 % of a frame.
  The scene constants hold what shaders need of the cascades. `ShadowSettings::Distance` is how far shadows reach.
- **Fitted cascades.** The cascades are shared out over the stretch of the view that holds something, not over
  everything up to the shadow distance (`ShadowSettings::FitsToView`). `Passes/DepthRangePass` finds the nearest
  and farthest depth of the finished depth buffer with a compute shader, and the CPU reads them three frames later
  (`RenderData::SeenDepthRange`, a `ViewDepthRange`, for anything that wants to know). `ShadowFit` turns that into
  the stretch to fit to: its ends move in steps of a quarter doubling and only give way after two steps, because a
  cascade that changes size shows every shadow edge differently. All cascades but the last share the stretch out
  by ratio. The last always reaches from the camera to the shadow distance, so what comes into view before the
  measurement catches up still has a shadow. Finer near cascades draw more: small scenes pay up to 0.2 ms for it.
- **Soft shadows.** The edge of a shadow is sharp at its caster and widens with the distance from it, by
  the size of the sun, `DirectionalLightComponent::AngularSize` (`cascadeVisibility` in `StandardShading.slang`, percentage-closer soft shadows).
  Eight reads of four depths each find how far away the caster is, sixteen comparisons over a disc of that width
  give the shadow. The disc is turned by a different angle per pixel, which shows as fine grain until TAA exists.
  Every point is compared with the depth the surface has there, worked out from its normal without the normal
  map, or a wide disc would put a slanted surface into its own shadow. The width is kept between 1.5 and 12
  texels, so far cascades are softer than the sun makes them.
- **Sky.** The sky is worked out from the air and the sun (`Renderer/Atmosphere.h`, `Atmosphere.slang`, after
  Hillaire 2020), nothing is painted. The air is `AtmosphereSettings` in `Scene/Atmosphere.h`, which is itself
  the component "SkyAtmosphere": the first enabled one in a scene counts, a scene without one has the Earth's,
  and it reaches the renderer with the extracted scene. Three small tables, which the renderer owns and `Passes/AtmospherePass`
  fills: transmittance and multiple scattering once per `AtmosphereSettings`, the view of the sky from the camera
  every frame. `Passes/SkyPass` draws the sky, the sun's disc and the planet's bare ground where the depth buffer
  is empty, after the depth passes and before the forward pass. A view mode with raw colours has no sky.
  The sun's light on surfaces goes through the same air (`sunlightOnGround`), so a low sun is dim and red by
  itself. The shader works in kilometres with the camera above the planet's middle. Height zero of the scene is
  the planet's ground. The atmosphere is part of the scene constants.
- **Light from the sky.** `Passes/SkyLightPass` looks at the sky in 512 directions, in one group of compute
  threads, and writes nine numbers per colour (spherical harmonics) into a buffer the scene constants point to.
  `skyIrradiance(normal)` in `Atmosphere.slang` gives the light for a way of facing, which replaced the constant
  3 %. `Passes/SkyReflectionPass` draws what shiny surfaces mirror of the sky: a cube of 64 texels an edge in
  six sizes, each blurred for a rougher surface. `skyReflection(direction, roughness)` reads it, and `mirroredShare` in `StandardShading.slang`
  is the fitted curve for how much of it a surface mirrors. Both passes run only when the sun, the air or the
  camera's height (in steps of a quarter doubling) changed (`Passes/SkyInputs.h`). Nothing blocks either light
  yet: under a roof they are as bright as in the open, until ambient occlusion (step 3.11).
- **Occlusion.** The camera's view is culled in two phases (`CullPhase`), so what is hidden behind other things is
  not drawn. Early: what was seen in the last frame, kept as a flag per instance on the GPU. Its depth becomes the
  depth pyramid (`Passes/DepthPyramidPass`): the depth in ever smaller sizes, each pixel the farthest of those it
  stands for. Late: every instance is tested against the pyramid, and what is not hidden and was not drawn early
  is drawn. So the depth pass runs twice ("Depth", "DepthLate") and a batch has two camera commands, which the
  forward pass draws as one range. Nothing visible is ever left out. An orthographic camera skips the test.
- **Uploads.** Data for buffers and pixels for textures go through the `UploadQueue`, which copies them to the GPU
  with the next frame. Nothing is submitted on its own.
- **Memory.** Textures come from a pool of their own (`GraphicsContext::TexturePool`), apart from buffers. Sharing
  blocks cost 7 to 10 % of GPU time in the mesh passes when measured.
- **Y axis.** The engine's matrices have +Y up, as GLM makes them. Vulkan has +Y down. Shaders get matrices that
  are turned around for that (`RenderData::ClipViewProjection`), and nothing else in a shader turns Y around.
- **Depth.** The scene's depth is reversed: 1 at the near plane, towards 0 at infinity, compared with "greater"
  and cleared to 0. Drawing has no far plane, the camera's own is only used for culling. The shadow map
  keeps the usual depth (0 near, "less").
- **Terrain.** A terrain is a square of 16-bit heights (`Asset/Terrain`), made in code or loaded from a file, put
  into a scene by a `TerrainComponent`, whose entity's position is the terrain's first corner. It is drawn as
  patches (continuous distance-dependent level of detail, Strugar 2010): `Renderer/TerrainRenderer` walks a tree
  of square patches on the CPU every frame and picks small ones near the camera and large ones far away, a few
  dozen in all. Ground is drawn however far away it is: the camera's far plane leaves no patch out (`Frustum::WithoutFarPlane`). Every patch is the same grid of 32 by 32 quads with no vertex data: `Terrain.slang` works a
  vertex's place out from its index, reads its height from the height map, and slides it towards the next
  coarser grid near the end of its level's distance, so levels meet without gaps or jumps. A patch that is
  only partly near enough for the finer level is drawn as quarters. `Passes/TerrainPass` draws the depth with
  the early depth pass, so ground hides things in the late culling, and the colour before the forward pass.
  The resolution has to be 32 times a power of two, plus one. Terrain casts shadows: the patches are picked
  once more per cascade, with the sun's view but as fine as the camera's distance asks for, so the shadow has
  the shape of the ground that is seen, and "TerrainShadow0" to "3" draw them after the meshes' shadow passes.
  Filling four maps with ground costs about 0.1 ms. What the ground looks like is up to four layers
  (`TerrainLayer`: a material asset and how many units one repeat of its textures covers) and a picture of
  weights, four bytes per place, that says how much of each shows where (`Terrain::SetLayers`). The layers'
  materials are entries of the material table, and the shader mixes colour, normal map, metalness and roughness
  by the weights, reading the textures by the place in the world. A layer is read at two sizes, as it is
  and eight times larger, and the larger one takes over as a repeat of the smaller shrinks below 256 pixels on
  screen, or a repeating texture shows as a pattern and then as one flat colour. Slow noise also makes the
  ground a little lighter and darker in patches of tens of units. A terrain without layers is drawn in two plain
  colours by slope. Inside, a terrain is cut into square tiles of 512 quads, each with its own pictures of
  heights and weights, for loading a landscape piece by piece later (stage 6). To whoever uses it, it is one
  asset with one list of layers, and all its tiles are loaded. The renderer treats every tile as a square of
  ground of its own. A tile's pictures hold one row of its neighbours' around them, so the light and the layers
  come out the same on both sides of an edge. A terrain's file is `.lxterrain`
  (`Asset/Serialization/TerrainSerializer`): a small text file with the size, the lowest and highest height, the
  tiling and the layers, and a folder `<name>.tiles` next to it with raw files per tile (`.r16` heights, `.rgba`
  weights). `Terrain::Save` writes one, the asset manager loads them, and a benchmark with `TerrainThroughFile=1`
  draws its terrain from a saved file. `TerrainSerializer::ReadHeights` reads a heightmap from a terrain tool
  (raw `.r16`, or a 16-bit PNG) for making a terrain.
  In the editor, "Create Terrain..." in the asset browser makes one from a heightmap, and the terrain's
  properties panel sets its layers and reads a picture of weights (`Terrain::SetLayers`, with or without new weights).
  The asset browser hides `.tiles` folders. Sculpting and painting in the editor do not exist yet.
- **Terrain physics.** A `TerrainColliderComponent` ("TerrainCollider") next to the terrain makes its ground solid, without a
  rigid body. `PhysicsSystem::UpdateTerrainColliders` makes one static Jolt height field per tile from the heights
  the tile is drawn from, and makes them again when the terrain is reloaded or its entity moved. The bodies are
  kept in the system, not in the component. A quad is split from its first corner to the one opposite in all
  three places: Jolt's height field, the patch grid of `TerrainRenderer`, and `Terrain::GetHeightAtPlace`, which
  gives the ground's height anywhere. "Show colliders" draws the triangles around the camera. A benchmark with
  `TerrainBalls=<count>` drops balls onto the terrain and shoots rays at it, and writes what it found to the log.
  Bodies made after a scene started exist from the next physics step on, and physics stands still while a
  benchmark warms up.
- **Lit surfaces.** Everything that is lit calls `lightOn(surface, pixel)` in `StandardShading.slang` with a
  `LitSurface` (position, normals, colour, metalness, roughness): the sun, its shadow, the sky. The view modes
  are `shownInPlaceOfLight` and `inLightAlone` next to it. A new kind of surface, like terrain, fills in a
  `LitSurface` and gets the same light and every view mode.
- **Exposure.** How bright the picture is made is one number the light of the scene is multiplied by.
  `Passes/ExposurePass` (`Exposure.slang`) counts how bright the scene's picture is into a histogram, one pixel
  in eight each way, and moves the exposure towards the one that shows the middle 80 % of it as a middle grey,
  faster towards darker than towards brighter. The exposure never leaves the GPU: it lives in a small buffer the
  scene constants point to (`scene.exposure`), and bloom and the composite pass multiply by it, so bloom comes
  after exposure. The settings are `ExposureSettings` in `Scene/Exposure.h`, itself the component "Exposure":
  automatic or fixed, compensation in stops, limits and speeds. A benchmark's frames have a fixed length, so its
  exposure is the same in every run. Views with raw colours have none. The tone curve is the fitted ACES curve
  with the factor of 0.6 it was fitted for, and the picture goes to the screen through the sRGB curve
  (`forScreen` in `Composite.slang`, undone by `shownAsItIs` for the view modes).
- **Cube textures.** `GpuTextureDesc::IsCube` makes six square faces that a shader reads by a direction
  (`cubeTextures[index]` in `Textures.slang`). Cubes have a table of their own in the shared bindings, with
  their own indices: `SharedBindings::AddCubeTexture`, and a `TextureSlot` made with `TextureTable::Cube`.
  With `ViewPerMip`, `FaceView(face, mip)` is one face of one size as a flat picture to draw into, and
  `cubeDirection(face, uv)` gives the direction a pixel of it stands for. Cubes cannot be loaded from files yet.
- **Texture colours.** Every RGBA8 texture is in the texture table twice. `Texture::GetColorTableIndex` reads it as
  sRGB and gives a shader linear light: use it for colours that are lit (albedo, emissive, particles).
  `GetTableIndex` gives the stored numbers: for normal maps, roughness, and pictures shown unlit (UI, ImGui).
  Shaders never convert with `pow`.
- **Vulkan.** The headers come from vcpkg (`vulkan-headers`). The engine does not link the Vulkan library: it
  loads the functions at run time through GLFW. NVRHI and shaderc are no longer part of the project.
- **ImGui.** Drawn by its official Vulkan backend, wrapped in `ImGui/ImGuiVulkan`. To show a texture, pass
  `Renderer::GetImGuiTexture(texture->GetTableIndex())` to `ImGui::Image`, cast to `ImTextureID`.
- **Picking.** `Renderer::RequestEntityIdAt` asks, `TakeEntityId` hands out the answer three frames later.

## Conventions

- New and changed code should be clean and clear even where the code around it is not. Do not copy old habits:
  no new `Engine::Get()` where a dependency can be passed in, no commented-out code, no unchecked `json["key"]`.
- New code follows the target design: lower layers do not reach up (Scene, Assets and UI do not call the renderer),
  editor-only code stays in the editor, and saved components hold no runtime state.
- Comments say why, not what. Members are `m_Name`, types and functions `PascalCase`, four spaces.
- Source files use CRLF line endings. Keep them when editing.
- Keep each step small and leave the editor and the game runnable after it.

## Verifying a change

- Build the whole preset before a commit, not one target: `tools/bench.ps1` builds RenderBench alone, and
  an editor that no longer compiles goes unnoticed (it happened on 26 November 2026).
- Build, then run the editor and the game and read `temp\engine.log` for `[error]`, `[critical]` and `VUID` lines.
- Rendering and speed: `tools/bench.ps1 -Label <name>` runs the RenderBench benchmarks in Release and stores timings
  and pictures under the label. Run it before and after a change, then `tools/bench-compare.ps1 -Before <a> -After <b>`.
  Pictures are identical between runs of the same code, so any difference comes from the change (Grid, Dense
  and Trees differ in a few pixels from run to run, which is known and not explained).
  A benchmark takes `TAA=1` (with `TaaHistoryTime`, `TaaBoxGamma`, `TaaSharpness`, `TaaDepthTolerance`),
  `Jitter=1`, `JitterLength=<frames>`, `Accumulate=1`, `FXAA=0`, and `HoldFrames=<count>`,
  which draws every saved frame that many more times with the game standing still before its picture is taken:
  `-Set Jitter=1,JitterLength=64,Accumulate=1,HoldFrames=63` gives pictures with 64 samples a pixel.
  The Motion benchmark is the scene anti-aliasing over time is judged on: Sponza, a sideways move of well
  under a pixel a frame, a nearly still stretch and a faster turn, with a box that keeps crossing the hall,
  bars of a pixel's width and wires thinner than a pixel (`MotionProps` in the benchmark's code).
  `tools/taa-score.ps1 -Truth <label> -Label <labels>` scores pictures against such 64-sample pictures of
  the same frames: the distance to them, the fine detail kept as a share of theirs (under 100 % is softer,
  over 100 % is jagged), and with `-Still <a>,<b>` (two labels held one frame apart) the flicker at rest.
  Judge a change to the temporal pass with it before trusting the eye. The timings compared
  are "clean": each benchmark runs five times and every frame counts with its fastest run, which removes the GPU time
  the Windows compositor takes at every display refresh. Clean GPU times repeat within about 1.3 %.
  "Frame" is the slower of two times, and "Bound by" says which: what the CPU needs for a frame without waiting for
  the GPU, and what the GPU needs. Only that limit says how fast a scene runs. Where the GPU is the limit, less CPU
  work shows in the "CPU" column and not in the frame. Results made before 21 November 2026 hold a frame time that
  reads too low for scenes bound by the GPU. Measure the "before" label right before the "after" one, the machine
  drifts over an hour. The Trees benchmark draws models that are not in the repository, 1.5 GB of them:
  `tools/fetch-polyhaven.ps1` fetches them, once per machine, and with them the three ground textures of the
  Terrain benchmark, 37 MB (`-NoModels` fetches the textures alone).
- The editor window: `tools/capture-window.ps1` saves a picture of it. Compare one from before with one from after.
- F3, or `--profiler` on the command line, shows frame, CPU and GPU times on screen in the editor and in a game.
- Serialization: re-save every scene and prefab with the old and the new code and compare the JSON.
- Editor interaction (clicking, dragging, pressing Play) cannot be automated. Say so, and ask for it to be checked.
- Running the game locks the mouse cursor on the machine for as long as it runs.

## Git

- Commit each finished step on `development`. Pushing is left to the developer.
- When a step turns out to be broken, commit the fix as its own commit after it. The history should show what
  went wrong.
- Images, model data and fonts are stored with Git LFS.
