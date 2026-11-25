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
- `samples/RenderBench/` the project the renderer is measured with: benchmark scenes, and one file per benchmark in `bench/`
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
  with `[shader("vertex")]`. Shared code lives in modules (`Scene`, `Textures`, `Mesh`, `Material`, `StandardShading`) that a
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
  A piece more than four times the usual leaf size, like a twig's stem, is left as it is until the leaves have
  grown to its size, and thinned out like them from there on. `LodSettings::LeafPixels` says
  how large a leaf may be on screen. An entry of the list of drawn instances holds the place in 24 bits and the
  level above them. A surface with cut-outs of few pieces, like a sail, keeps its one level.
- **View modes.** A view of a scene can show one thing about the picture instead of the picture (`ViewMode` in
  `Renderer/ViewMode.h`: wireframe, unlit, lighting only, normals, level of detail, shadow cascades) and draw cut-outs whole
  (`ViewSettings::ShowCutOuts`). The settings belong to the view and travel with it (`ExtractedScene::Viewing`),
  so two views can show a scene differently. `SceneRenderer::GetViewSettings` holds those of the main view, the
  viewport panel has the dropdown, and a benchmark takes `ViewMode=<number>` and `ShowCutOuts=0`. The forward
  shader does the modes, modes with raw colours skip bloom and tone mapping (`ShowsRawColours`). A new mode is a
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
  `ShadowSettings::SunAngle` (`cascadeVisibility` in `StandardShading.slang`, percentage-closer soft shadows).
  Eight reads of four depths each find how far away the caster is, sixteen comparisons over a disc of that width
  give the shadow. The disc is turned by a different angle per pixel, which shows as fine grain until TAA exists.
  Every point is compared with the depth the surface has there, worked out from its normal without the normal
  map, or a wide disc would put a slanted surface into its own shadow. The width is kept between 1.5 and 12
  texels, so far cascades are softer than the sun makes them.
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

- Build, then run the editor and the game and read `temp\engine.log` for `[error]`, `[critical]` and `VUID` lines.
- Rendering and speed: `tools/bench.ps1 -Label <name>` runs the RenderBench benchmarks in Release and stores timings
  and pictures under the label. Run it before and after a change, then `tools/bench-compare.ps1 -Before <a> -After <b>`.
  Pictures are identical between runs of the same code, so any difference comes from the change. The timings compared
  are "clean": each benchmark runs five times and every frame counts with its fastest run, which removes the GPU time
  the Windows compositor takes at every display refresh. Clean GPU times repeat within about 1.3 %.
  "Frame" is the slower of two times, and "Bound by" says which: what the CPU needs for a frame without waiting for
  the GPU, and what the GPU needs. Only that limit says how fast a scene runs. Where the GPU is the limit, less CPU
  work shows in the "CPU" column and not in the frame. Results made before 21 November 2026 hold a frame time that
  reads too low for scenes bound by the GPU. Measure the "before" label right before the "after" one, the machine
  drifts over an hour. The Trees benchmark draws models that are not in the repository, 1.5 GB of them:
  `tools/fetch-polyhaven.ps1` fetches them, once per machine.
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
