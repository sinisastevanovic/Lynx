# Gemini Codebase Analysis: LynxEngine

This document summarizes the architecture, key components, and current state of the LynxEngine, a C++23 3D game engine.

## Project Overview

- **Engine Name:** LynxEngine
- **Language:** C++23
- **Core Architecture:** Modular design with a core engine library, an editor, and game logic split into a hot-reloadable DLL and a standalone executable.
- **Build System:** CMake.
- **Dependency Management:** `vcpkg` (manifest mode).

## Key Architectural Patterns

- **Modules:**
    - `engine`: Core runtime library (Static Mesh, Textures, ECS, Physics, Renderer).
    - `editor`: Editor application (ImGui-based) that loads the game DLL.
    - `game_dll`: Hot-reloadable game logic module.
    - `game_standalone`: Monolithic executable for the final game.
- **Game Loop:** The `Engine` class controls the main loop, delegating update/render calls to the active `Scene` and `IGameModule`.
- **ECS (Entity Component System):** Built on `entt`.
    - **Engine Components:** `IDComponent` (UUID), `TagComponent`, `TransformComponent`, `MeshComponent`, `CameraComponent`. Defined in `engine/src/Lynx/Scene/Components/Components.h`.
    - **Game Components:** `PlayerComponent`, `EnemyComponent`, `EnemySpawnerComponent`, `WeaponComponent`, `ProjectileComponent`. Defined in `game/src/Components/GameComponents.h`.
- **Asset System:**
    - Uses `AssetManager` and `AssetRegistry`.
    - Assets are tracked via UUIDs and metadata files (`.lxmeta`).
    - Supports `StaticMesh` (.gltf/.bin) and `Texture` (.png).
- **Rendering:**
    - Abstraction: **NVRHI** (NVIDIA Rendering Hardware Interface) over Vulkan.
    - Resources are managed via `AssetManager`.
- **Physics:**
    - **Jolt Physics** integration.
    - `PhysicsSystem` manages the simulation.

## Core Dependencies (`vcpkg.json`)

- **`entt`**: ECS.
- **`glfw3`**: Windowing and input.
- **`nvrhi`**: Graphics API abstraction.
- **`imgui`**: Editor UI.
- **`joltphysics`**: Physics engine.
- **`sol2`**: Lua scripting binding (Lua integration in progress).
- **`spdlog`**: Logging.
- **`glm`**: Mathematics (Vectors, Matrices, Quaternions).
- **`tinygltf`**: GLTF model loading.
- **`stb`**: Image loading.
- **`nlohmann-json`**: Serialization.

## Important Files

- **Entry Points:**
    - `engine/src/Lynx/Engine.h`: Main engine class definition.
    - `game/src/MyGameModule.cpp`: Game logic implementation.
- **Data Definitions:**
    - `engine/src/Lynx/Scene/Components/Components.h`: Core ECS components.
    - `game/src/Components/GameComponents.h`: Gameplay ECS components.
- **Systems:**
    - `engine/src/Lynx/Asset/AssetManager.h`: Asset loading and caching.
    - `engine/src/Lynx/Scene/Scene.h`: Scene graph and entity management.
    - `engine/src/Lynx/Renderer/Renderer.h`: Rendering commands.

## Current State & Roadmap

Based on `Roadmap.md` and codebase analysis.

### Phase 1: Foundation & Rendering (Complete)
- [x] Window & Input (GLFW)
- [x] Renderer V1 (NVRHI, Basic Shaders)
- [x] 3D Space & Camera (GLM, Editor Camera)
- [x] ECS Integration (entt)
- [x] Asset System V1 (Loading Models/Textures, Asset Registry)

### Phase 2: Core Gameplay (In Complete)
- [x] Physics & Collisions (Jolt Physics)
- [x] Player Control (`PlayerComponent`, `PlayerSystem`)
- [x] Enemy Spawning & AI (`EnemyComponent`, `SpawnerSystem`)
- [x] Weapons & Damage (`WeaponComponent`, `ProjectileComponent`)

### Phase 3: Editor Tools (In Complete)
- [x] Editor Viewport (ImGui, Framebuffer)
- [x] Scene Hierarchy & Inspector
- [x] Gizmos (ImGuizmo)
- [x] Scene Serialization (JSON)
- [x] Asset Browser (Visual)
- [x] Scripting (Lua/Sol2 integration pending)

### Phase 4: The Visual Upgrade (In Progress)
- [x] Asset Pipeline Upgrade
- [x] Renderer V2 (PBR & Lighting)
- [x] Debug Tools
- [x] Optimization (Instancing, Depth Pre-Pass, Async Loading)
- [x] Post-Processing (Bloom, FXAA, Tone Mapping)
- [x] Texture Quality (Mipmaps, Anisotropy)
- [x] Visual Effects (Particle System)

### Phase 5: Polish & Shipping (Planned)
- [ ] Fix Transforms (Hierarchy issues)
- [ ] Fix Editor Order (Component/Entity sorting)
- [ ] Optimization (EnTT Groups)
- [ ] Optimization (EnTT Sort for Rendering)
- [ ] Game UI (HUD)
- [ ] Audio (miniaudio)
- [ ] Advanced Scripting (Physics events, spawning)
- [ ] Distributable Build
