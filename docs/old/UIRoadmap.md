# LynxUI Roadmap

This document outlines the design and implementation plan for the LynxEngine UI system. The system is designed to be high-performance, flexible (material-aware), and editor-friendly.

## Core Principles

1.  **Hybrid Architecture:** 
    - Use ECS (`UICanvasComponent`) for high-level scene integration.
    - Use a dedicated C++ object tree (`UIElement`) for internal hierarchy, layout, and input to ensure performance and ease of use.
2.  **DPI Awareness:** 
    - All user-facing values (size, padding, offset) are in **dp (density-independent pixels)**.
    - 160 dpi = 1.0 scale factor.
3.  **Pixel Perfection:**
    - The layout engine will perform "Pixel Snapping" during the Arrange phase to ensure borders and text remain sharp.
4.  **Material-Aware Rendering:**
    - Elements can have custom materials (shaders).
    - The renderer will use a "Smart Batcher" that merges consecutive elements sharing the same material.
5.  **Editor-First:**
    - Every UI element must be serializable to JSON.
    - Every UI element must implement `OnInspect()` for the Editor property grid.

---

## Phase 1: Foundation (The Core)
*   [x] **Directory Structure:** Create `engine/src/Lynx/UI` (Core, Widgets, Rendering, System).
*   [x] **Base Class (`UIElement`):** 
    - Hierarchy (Parent/Children).
    - Transform (Anchors, Pivot, Offset, Size).
    - Visibility and Opacity.
    - Slot for `Material`.
*   [x] **ECS Integration:** 
    - Create `UICanvasComponent`.
    - Update `Scene` to manage Canvas updates.
*   [x] **Serialization:** Implement basic JSON save/load for the UI tree.

## Phase 2: Layout & Geometry
*   [x] **Math Helpers:** Implement `Rect`, `Thickness` (Padding/Margin), and `Anchor` logic.
*   [x] **The Two-Pass Layout:**
    - **Measure Pass (Bottom-Up):** Elements calculate their desired size.
    - **Arrange Pass (Top-Down):** Parents set the final bounds for children, applying pixel snapping.
*   [x] **Layout Widgets:** 
    - `CanvasPanel` (Manual positioning).
    - `StackPanel` (Horizontal/Vertical).

## Phase 3: Rendering Pipeline
*   [x] **UIPass:** Add a specialized `UIPass` to the `Renderer` pipeline.
*   [x] **UIBatcher:** 
    - Collect quads from the UI tree.
    - Generate dynamic Vertex/Index buffers.
    - Implement batch flushing on material changes.
*   [ ] **Internal Shaders:**
    - `UI_Standard`: Simple texture/color/clipping.
    - `UI_SDF`: High-quality text rendering.

## Phase 4: Sprites & 9-Slice
*   [x] **Sprite Asset:** Create a `Sprite` class that defines a sub-region (UVs) of a `Texture`.
*   [x] **9-Slice Logic:** Implement 9-slice mesh generation in the `UIImage` widget.
*   [x] **Asset Integration:** Allow `AssetManager` to handle Sprite metadata.

## Phase 5: Text & Fonts
*   [x] **SDF Font Pipeline:** Implement an importer for SDF font textures and metadata.
*   [x] **`UIText` Widget:** 
    - Text alignment.
    - Dynamic string updates.
    - Color and font size (dp).

## Phase 6: Interaction (Events)
*   [x] **Input Handling:** 
    - Translate mouse/touch coordinates into UI space.
    - Perform hit-testing against the UI tree.
*   [x] **Event Routing:** Implement **Bubbling** (Target -> Root) for events like `OnClick`, `OnHover`, `OnScroll`.

## Phase 7: Editor Tools (The "Must Have")
*   [x] **UI Hierarchy Panel:** A tree view of the UI elements in the current scene.
*   [x] **UI Inspector:** Property editing for the selected element (anchors, colors, materials).
*   [x] **Viewport Gizmos:** Visual anchors and rect handles in the Editor.

## Phase 7.5: Advanced Widgets & Gameplay UI (The "Vampire Survivors" Pack)
*   [x] **Progress Bar (Filled Image):** Extend `UIImage` to support `FillAmount` (Horizontal/Vertical/Radial) for Health/XP bars.
*   [x] **ScrollView & Masking:** Implement a container that clips content and allows scrolling (needed for lists).
*   [ ] **Layouts (Grid/Wrap):** `GridPanel` or `WrapPanel` for inventory/character selection screens.
*   [x] **World-Space UI:** Logic to map 3D world positions to 2D UI coordinates (Floating Damage Numbers).
*   [ ] **Controller Navigation:** (Optional V2) Focus system for navigating UI with keyboard/gamepad.

## Phase 8: Polish & Optimization (Optional/Future)
*   [ ] **Text Improvements**: Not really optimization, but Multi-line Text, Rich Text, SDF Effects (Outline, drop shadows)
*   [x] **Pixel Perfect Rendering**: Are we doing this??
*   [ ] **Overlap-Aware Reordering:** Batch materials even if they aren't consecutive, provided they don't overlap.
*   [ ] **Texture Atlasing:** Automatic runtime atlasing for UI textures.
