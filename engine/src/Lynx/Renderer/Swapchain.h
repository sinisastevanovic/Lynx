#pragma once

#include <nvrhi/nvrhi.h>
#include <vulkan/vulkan.hpp>

#include <vector>

namespace Lynx
{
    class GraphicsContext;

    struct SwapchainSettings
    {
        // Present in step with the display refresh. Off presents as fast as the GPU allows,
        // without tearing where the driver supports it.
        bool VSync = true;

        // Ask for one image more than the driver's minimum. With VSync the display holds on to an image
        // for a whole refresh, and without a spare one the CPU waits until it is released. The spare image
        // lets it start the next frame at once, at the price of memory and of a frame that may wait in line.
        bool ExtraImage = false;

        bool operator==(const SwapchainSettings&) const = default;
    };

    // The pictures a window cycles through: the frame is drawn into one of them and then shown.
    class Swapchain
    {
    public:
        // The size is used only where the window system leaves the size to the application.
        // Throws std::runtime_error if the window cannot show the format the renderer presents.
        Swapchain(GraphicsContext& context, uint32_t width, uint32_t height, const SwapchainSettings& settings);
        ~Swapchain();

        Swapchain(const Swapchain&) = delete;
        Swapchain& operator=(const Swapchain&) = delete;

        const SwapchainSettings& Settings() const { return m_Settings; }

        // Both wait for the GPU and replace the swapchain, so every framebuffer handed out before is gone.
        void ApplySettings(const SwapchainSettings& settings);
        void Resize(uint32_t width, uint32_t height);

        uint32_t Width() const { return m_Extent.width; }
        uint32_t Height() const { return m_Extent.height; }

        // Gets the image the next frame is drawn into. The semaphore is signalled once the image may be written.
        // A swapchain that no longer matches its window is replaced first. Returns false if no image could be had,
        // in which case the frame has to be skipped.
        bool AcquireImage(vk::Semaphore imageAvailable);

        // The framebuffer of the acquired image.
        nvrhi::IFramebuffer* Framebuffer() const { return m_Framebuffers[m_ImageIndex]; }

        // The frame's last submission has to signal this. Present waits for it.
        vk::Semaphore RenderFinished() const { return m_RenderFinished[m_ImageIndex]; }

        // Shows the acquired image.
        void Present();

    private:
        void Create(uint32_t width, uint32_t height);
        void Destroy();

    private:
        GraphicsContext& m_Context;
        SwapchainSettings m_Settings;

        vk::SwapchainKHR m_Swapchain;
        vk::Extent2D m_Extent;
        std::vector<nvrhi::FramebufferHandle> m_Framebuffers;
        std::vector<vk::Semaphore> m_RenderFinished;
        uint32_t m_ImageIndex = 0;

        // Set when the driver reported that the swapchain no longer matches the window.
        bool m_IsStale = false;
    };
}
