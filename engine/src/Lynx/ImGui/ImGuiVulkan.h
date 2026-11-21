#pragma once

#include <imgui.h>
#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace Lynx
{
    class GraphicsContext;
    class SharedBindings;

    // Draws ImGui with its official Vulkan backend: the editor's whole interface, and in a game the overlays.
    //
    // ImGui names a texture by a descriptor set of the backend's own. This class makes one for a texture
    // the first time ImGui is to show it, and gives it up when the texture leaves the renderer's texture table.
    class ImGuiVulkan
    {
    public:
        // The ImGui context and its platform backend have to exist already.
        ImGuiVulkan(GraphicsContext& context, SharedBindings& shared, vk::Format windowFormat, vk::Sampler sampler);

        // Has to run before the ImGui context is destroyed.
        ~ImGuiVulkan();

        ImGuiVulkan(const ImGuiVulkan&) = delete;
        ImGuiVulkan& operator=(const ImGuiVulkan&) = delete;

        // Call before ImGui::NewFrame.
        void NewFrame();

        // Draws what ImGui::Render collected. Rendering into the window's picture has to have begun.
        void Render(vk::CommandBuffer commands);

        // Draws and shows the panels that were dragged out of the main window into windows of their own.
        // Call after the frame was submitted.
        void RenderOtherWindows();

        // What ImGui::Image takes for a texture of the renderer's texture table.
        // The texture has to be in the layout for reading whenever ImGui draws it.
        ImTextureID TextureId(uint32_t tableIndex);

    private:
        void Forget(uint32_t tableIndex);

    private:
        GraphicsContext& m_Context;
        SharedBindings& m_Shared;
        vk::Sampler m_Sampler;

        // The backend's descriptor set of every texture ImGui has shown, by the texture's index in the table.
        std::vector<VkDescriptorSet> m_Sets;

        // False once the backend is shut down, for sets whose removal is still waiting in the deletion queue.
        std::shared_ptr<bool> m_BackendAlive;
    };
}
