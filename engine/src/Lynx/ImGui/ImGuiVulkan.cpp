#include "ImGuiVulkan.h"

#include "Lynx/Log.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

#include <imgui_impl_vulkan.h>

namespace Lynx
{
    namespace
    {
        // How many textures ImGui can show at once: the editor's pictures and the thumbnails of a folder.
        constexpr uint32_t MaxShownTextures = 4096;

        // The window's pictures the backend sizes its own buffers by. It only needs "more than one frame".
        constexpr uint32_t BufferedFrames = 3;
    }

    ImGuiVulkan::ImGuiVulkan(GraphicsContext& context, SharedBindings& shared, vk::Format windowFormat, vk::Sampler sampler)
        : m_Context(context), m_Shared(shared), m_Sampler(sampler), m_BackendAlive(std::make_shared<bool>(true))
    {
        // The engine does not link the Vulkan library. The backend gets its functions from the same loader the renderer uses.
        const bool loaded = ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, [](const char* name, void* instance)
        {
            return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(static_cast<VkInstance>(instance), name);
        }, static_cast<VkInstance>(context.Instance()));
        if (!loaded)
            LX_CORE_ERROR("ImGui could not load the Vulkan functions it needs.");

        const VkFormat colorFormat = static_cast<VkFormat>(windowFormat);
        VkPipelineRenderingCreateInfoKHR renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachmentFormats = &colorFormat;

        ImGui_ImplVulkan_InitInfo info{};
        info.ApiVersion = VK_API_VERSION_1_3;
        info.Instance = context.Instance();
        info.PhysicalDevice = context.PhysicalDevice();
        info.Device = context.Device();
        info.QueueFamily = context.GraphicsQueueFamily();
        info.Queue = context.GraphicsQueue();
        info.DescriptorPoolSize = MaxShownTextures;
        info.MinImageCount = 2;
        info.ImageCount = BufferedFrames;
        info.UseDynamicRendering = true;
        info.PipelineInfoMain.PipelineRenderingCreateInfo = renderingInfo;

        // Windows of their own get a swapchain from the backend, which asks for this format first.
        info.PipelineInfoForViewports.PipelineRenderingCreateInfo = renderingInfo;
        info.CheckVkResultFn = [](VkResult result)
        {
            if (result != VK_SUCCESS)
                LX_CORE_ERROR("ImGui Vulkan backend: {0}", vk::to_string(static_cast<vk::Result>(result)));
        };

        if (!ImGui_ImplVulkan_Init(&info))
            LX_CORE_ERROR("The ImGui Vulkan backend could not be started.");

        m_Shared.SetTextureReleaseListener([this](uint32_t tableIndex) { Forget(tableIndex); });
    }

    ImGuiVulkan::~ImGuiVulkan()
    {
        m_Shared.SetTextureReleaseListener({});
        *m_BackendAlive = false;

        // The renderer waits for the GPU before it destroys this, and the backend frees all its sets with its pool.
        ImGui_ImplVulkan_Shutdown();
    }

    void ImGuiVulkan::NewFrame()
    {
        ImGui_ImplVulkan_NewFrame();
    }

    void ImGuiVulkan::Render(vk::CommandBuffer commands)
    {
        if (ImDrawData* drawData = ImGui::GetDrawData())
            ImGui_ImplVulkan_RenderDrawData(drawData, commands);
    }

    void ImGuiVulkan::RenderOtherWindows()
    {
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    ImTextureID ImGuiVulkan::TextureId(uint32_t tableIndex)
    {
        if (m_Sets.size() <= tableIndex)
            m_Sets.resize(tableIndex + 1, VK_NULL_HANDLE);

        if (!m_Sets[tableIndex])
            m_Sets[tableIndex] = ImGui_ImplVulkan_AddTexture(m_Sampler, m_Shared.TextureView(tableIndex), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(m_Sets[tableIndex]));
    }

    void ImGuiVulkan::Forget(uint32_t tableIndex)
    {
        if (tableIndex >= m_Sets.size() || !m_Sets[tableIndex])
            return;

        // A frame on the GPU may still draw with the set.
        m_Context.Deletions().Retire([alive = m_BackendAlive, set = m_Sets[tableIndex]]()
        {
            if (*alive)
                ImGui_ImplVulkan_RemoveTexture(set);
        });
        m_Sets[tableIndex] = VK_NULL_HANDLE;
    }
}
