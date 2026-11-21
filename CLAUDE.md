# LynxEngine

A hobby 3D game engine in C++23: Vulkan 1.3 through a thin layer of its own, EnTT, Jolt physics, Lua through sol2, an ImGui editor.
One developer. The engine was reviewed in November 2026 and is being refactored step by step, not rewritten.

Current state, open problems and the order of work: https://claude.ai/artifact/8UigvboHdEzfD3iVqeeeyy
Read its "Status" section before starting anything larger. Its "Target design" section describes where the
structure is heading, and "Toward the target design" the order of the steps.

Renderer work follows its own plan, Renderer V2: https://claude.ai/artifact/SShPW5mVGsxRJYgqVdASKj
It is the main line of work. Read its "Decisions" and "Status" sections before touching the renderer, and tick off
the steps under "Stage 2 in detail" as they land. Stage 1 is complete. The non-renderer steps of the review are paused meanwhile.

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
- The log is written to `<project>\temp\engine.log`. The shader cache and editor layout are in the same folder.
- Only Debug and Release exist. Release has not been tested recently.

## How things work

- **Paths.** Nothing depends on the working directory. Ask `ContentPaths` for a location, and use
  `AssetManager::GetEngineAsset` or `GetProjectAsset` for assets.
- **Components.** Register a component with a field list (`Reflection/Field.h`). Saving, loading, the inspector and
  Lua access come from it. The first argument of `fields.Add` is the key in saved files: never rename it without
  migrating the scenes. Only components with real custom logic keep hand-written save and load code.
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
- **Geometry.** The vertices and indices of all meshes are in one pair of buffers (`Vulkan/GeometryPool`). A submesh
  is a range in them (`MeshGeometry`). A pass binds the pool once and draws ranges. The buffers grow when they fill up.
- **Mesh draws.** Meshes that are not see-through are drawn from a buffer of draw commands, one per submesh with all
  its instances (`RenderData::MeshDraws`). A pass draws a whole range of them with one call (`DrawMeshes`). The CPU
  still culls and writes the commands. Depth and shadow use a pipeline without a fragment shader for opaque
  materials and a second one for cut-outs. See-through meshes are sorted and drawn one by one.
- **Uploads.** Data for buffers and pixels for textures go through the `UploadQueue`, which copies them to the GPU
  with the next frame. Nothing is submitted on its own.
- **Memory.** Textures come from a pool of their own (`GraphicsContext::TexturePool`), apart from buffers. Sharing
  blocks cost 7 to 10 % of GPU time in the mesh passes when measured.
- **Y axis.** The engine's matrices have +Y up, as GLM makes them. Vulkan has +Y down. Shaders get matrices that
  are turned around for that (`RenderData::ClipViewProjection`), and nothing else in a shader turns Y around.
- **Depth.** The scene's depth is reversed: 1 at the near plane, towards 0 at infinity, compared with "greater"
  and cleared to 0. The GPU has no far plane, the camera's own is only used for culling on the CPU. The shadow map
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
  the Windows compositor takes at every display refresh. Clean GPU times repeat within about 1.3 % and the clean frame
  time within 2 %. Measure the "before" label right before the "after" one, the machine drifts over an hour.
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
