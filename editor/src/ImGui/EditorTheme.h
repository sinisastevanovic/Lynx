#pragma once
#include <imgui.h>

namespace Lynx::EditorTheme
{
    // UE5-like Palette
    constexpr ImVec4 ColorBg           = ImVec4(0.11f, 0.11f, 0.11f, 1.0f); // Main Window Background
    constexpr ImVec4 ColorPanel        = ImVec4(0.15f, 0.15f, 0.15f, 1.0f); // Panel/Tab Background
    constexpr ImVec4 ColorInput        = ImVec4(0.05f, 0.05f, 0.05f, 1.0f); // Input fields (Deep dark)

    constexpr ImVec4 ColorHighlight    = ImVec4(0.25f, 0.25f, 0.25f, 1.0f); // Hover/Interaction
    constexpr ImVec4 ColorActive       = ImVec4(0.35f, 0.35f, 0.35f, 1.0f); // Active/Selected

    constexpr ImVec4 ColorText         = ImVec4(0.90f, 0.90f, 0.90f, 1.0f); // Bright Text
    constexpr ImVec4 ColorTextMuted    = ImVec4(0.60f, 0.60f, 0.60f, 1.0f); // Muted Text

    // UE5 Selection Blue (optional, or stick to grey for a strict look)
    constexpr ImVec4 ColorSelection    = ImVec4(0.0f, 0.44f, 0.88f, 1.0f);

    inline void ApplyTheme()
    {
        auto& style = ImGui::GetStyle();
        auto& colors = style.Colors;

        // Modern, sharp rounding
        style.WindowRounding    = 4.0f;
        style.FrameRounding     = 2.0f;
        style.GrabRounding      = 2.0f;
        style.TabRounding       = 4.0f;
        style.PopupRounding     = 4.0f;

        // Sizing
        style.FramePadding      = ImVec2(6, 4);
        style.ItemSpacing       = ImVec2(8, 4);

        // --- Colors ---

        // Text
        colors[ImGuiCol_Text] = ColorText;
        colors[ImGuiCol_TextDisabled] = ColorTextMuted;

        // Windows & Backgrounds
        colors[ImGuiCol_WindowBg] = ColorBg;
        colors[ImGuiCol_ChildBg] = ColorBg;
        colors[ImGuiCol_PopupBg] = ColorPanel;
        colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        // Headers (Collapsing headers, tree nodes)
        colors[ImGuiCol_Header] = ColorPanel;       // Stand out slightly from BG
        colors[ImGuiCol_HeaderHovered] = ColorHighlight;
        colors[ImGuiCol_HeaderActive] = ColorActive;

        // Buttons
        colors[ImGuiCol_Button] = ColorPanel;
        colors[ImGuiCol_ButtonHovered] = ColorHighlight;
        colors[ImGuiCol_ButtonActive] = ColorActive;

        // Inputs (Text fields, drags) - darker than bg to look "inset"
        colors[ImGuiCol_FrameBg] = ColorInput;
        colors[ImGuiCol_FrameBgHovered] = ColorPanel; // Light up slightly
        colors[ImGuiCol_FrameBgActive] = ColorHighlight;

        // Tabs (Docking)
        colors[ImGuiCol_Tab] = ColorBg;              // Inactive tabs merge with BG
        colors[ImGuiCol_TabHovered] = ColorHighlight;
        colors[ImGuiCol_TabActive] = ColorPanel;     // Active tab pops forward
        colors[ImGuiCol_TabUnfocused] = ColorBg;
        colors[ImGuiCol_TabUnfocusedActive] = ColorPanel;

        // Title Bars
        colors[ImGuiCol_TitleBg] = ColorBg;
        colors[ImGuiCol_TitleBgActive] = ColorBg; // Keep it flat
        colors[ImGuiCol_TitleBgCollapsed] = ColorBg;

        // Scrollbars
        colors[ImGuiCol_ScrollbarBg] = ColorBg;
        colors[ImGuiCol_ScrollbarGrab] = ColorPanel;
        colors[ImGuiCol_ScrollbarGrabHovered] = ColorHighlight;
        colors[ImGuiCol_ScrollbarGrabActive] = ColorActive;

        // Selection (Text select, etc)
        colors[ImGuiCol_TextSelectedBg] = ImVec4(ColorSelection.x, ColorSelection.y, ColorSelection.z, 0.35f);
        colors[ImGuiCol_CheckMark] = ColorSelection;
        colors[ImGuiCol_SliderGrab] = ColorHighlight;
        colors[ImGuiCol_SliderGrabActive] = ColorActive;

        // Docking Separator
        colors[ImGuiCol_Separator]        = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

        // Make the hover/active states VERY visible (UE5 Blue)
        colors[ImGuiCol_SeparatorHovered] = ColorSelection;
        colors[ImGuiCol_SeparatorActive]  = ColorSelection;

        // Resize Grip
        colors[ImGuiCol_ResizeGrip] = ImVec4(0,0,0,0);
        colors[ImGuiCol_ResizeGripHovered] = ColorSelection;
        colors[ImGuiCol_ResizeGripActive]  = ColorSelection;
        
        colors[ImGuiCol_DragDropTarget] = ColorSelection;
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}