# LynxEngine

A hobby 3D game engine in C++23: Vulkan through NVRHI, EnTT, Jolt physics, Lua through sol2, an ImGui editor.
One developer. The engine was reviewed in November 2026 and is being refactored step by step, not rewritten.

Current state, open problems and the order of work: https://claude.ai/artifact/8UigvboHdEzfD3iVqeeeyy
Read its "Status" section before starting anything larger. Its "Target design" section describes where the
structure is heading, and "Toward the target design" the order of the steps.

Renderer work follows its own plan, Renderer V2: https://claude.ai/artifact/SShPW5mVGsxRJYgqVdASKj
It is the main line of work. Read its "Decisions" and "Status" sections before touching the renderer, and tick off
the steps under "Stage 1 in detail" as they land. The non-renderer steps of the review are paused meanwhile.

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
  - `Renderer/` renderer and `Passes/`
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
- **Renderer.** NVRHI validation is on in debug builds. A new binding layout or set that is wrong shows up in the log
  as `NVRHI:` errors right at startup.

## Conventions

- New and changed code should be clean and clear even where the code around it is not. Do not copy old habits:
  no new `Engine::Get()` where a dependency can be passed in, no commented-out code, no unchecked `json["key"]`.
- New code follows the target design: lower layers do not reach up (Scene, Assets and UI do not call the renderer),
  editor-only code stays in the editor, and saved components hold no runtime state.
- Comments say why, not what. Members are `m_Name`, types and functions `PascalCase`, four spaces.
- Source files use CRLF line endings. Keep them when editing.
- Keep each step small and leave the editor and the game runnable after it.

## Verifying a change

- Build, then run the editor and the game and read `temp\engine.log` for `[error]`, `[critical]` and `NVRHI:` lines.
- Rendering and speed: `tools/bench.ps1 -Label <name>` runs the RenderBench benchmarks in Release and stores timings
  and pictures under the label. Run it before and after a change, then `tools/bench-compare.ps1 -Before <a> -After <b>`.
  Pictures are identical between runs of the same code, so any difference comes from the change. Frame times repeat
  within about 2.5 % when both labels are measured back to back, and drift by up to 5 % over an hour, so measure the
  "before" label right before the "after" one. In the grid benchmark the GPU times jump between two values about 8 %
  apart from run to run: trust its frame time, and confirm a GPU verdict there with a second run.
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
