#include "Swapchain.h"

#include "GraphicsContext.h"
#include "Lynx/Log.h"

#include <algorithm>
#include <stdexcept>

namespace Lynx
{
    namespace
    {
        // The composite pass and the ImGui layer write this format, so there is nothing to fall back to yet.
        constexpr vk::Format ImageFormat = vk::Format::eB8G8R8A8Unorm;
        constexpr vk::ColorSpaceKHR ImageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;

        vk::PresentModeKHR ChoosePresentMode(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface, bool vsync)
        {
            // FIFO waits for the display refresh, and every Vulkan implementation supports it.
            if (vsync)
                return vk::PresentModeKHR::eFifo;

            // Without VSync, mailbox is preferred because it does not tear. Immediate does.
            const std::vector<vk::PresentModeKHR> supportedModes = physicalDevice.getSurfacePresentModesKHR(surface);
            for (vk::PresentModeKHR preferredMode : { vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eImmediate })
            {
                if (std::ranges::find(supportedModes, preferredMode) != supportedModes.end())
                    return preferredMode;
            }

            return vk::PresentModeKHR::eFifo;
        }
    }

    Swapchain::Swapchain(GraphicsContext& context, uint32_t width, uint32_t height, const SwapchainSettings& settings)
        : m_Context(context), m_Settings(settings)
    {
        const std::vector<vk::SurfaceFormatKHR> formats = m_Context.PhysicalDevice().getSurfaceFormatsKHR(m_Context.Surface());
        const bool hasFormat = std::ranges::any_of(formats, [](const vk::SurfaceFormatKHR& format)
        {
            return format.format == ImageFormat && format.colorSpace == ImageColorSpace;
        });
        if (!hasFormat)
        {
            LX_CORE_CRITICAL("The window cannot show 8-bit BGRA pictures, which is the only format the renderer presents.");
            throw std::runtime_error("Unsupported surface format");
        }

        Create(width, height);
    }

    Swapchain::~Swapchain()
    {
        m_Context.WaitIdle();
        Destroy();
    }

    void Swapchain::ApplySettings(const SwapchainSettings& settings)
    {
        if (settings == m_Settings)
            return;

        // The present mode and the image count are fixed when a swapchain is created.
        m_Settings = settings;
        Resize(m_Extent.width, m_Extent.height);
    }

    void Swapchain::Resize(uint32_t width, uint32_t height)
    {
        m_Context.WaitIdle();
        Destroy();
        Create(width, height);
    }

    bool Swapchain::AcquireImage(vk::Semaphore imageAvailable)
    {
        // A swapchain stops matching its window when the window is resized or moved to another display.
        // The driver then refuses to hand out images, and a new swapchain has to be made.
        if (m_IsStale)
            Resize(m_Extent.width, m_Extent.height);

        // The overload that takes a pointer reports an out-of-date swapchain as a result. The other one throws.
        const auto acquire = [this, imageAvailable]()
        {
            return m_Context.Device().acquireNextImageKHR(m_Swapchain, UINT64_MAX, imageAvailable, nullptr, &m_ImageIndex);
        };

        vk::Result acquired = acquire();
        if (acquired == vk::Result::eErrorOutOfDateKHR)
        {
            Resize(m_Extent.width, m_Extent.height);
            acquired = acquire();
        }

        // Suboptimal still gives a usable image. The swapchain is replaced before the next frame.
        if (acquired == vk::Result::eSuboptimalKHR)
        {
            m_IsStale = true;
            return true;
        }

        if (acquired != vk::Result::eSuccess)
        {
            LX_CORE_ERROR("Could not acquire a swapchain image: {0}", vk::to_string(acquired));
            return false;
        }
        return true;
    }

    void Swapchain::Present()
    {
        vk::PresentInfoKHR presentInfo;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_Swapchain;
        presentInfo.pImageIndices = &m_ImageIndex;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_RenderFinished[m_ImageIndex];

        const vk::Result presented = m_Context.GraphicsQueue().presentKHR(&presentInfo);
        if (presented == vk::Result::eErrorOutOfDateKHR || presented == vk::Result::eSuboptimalKHR)
            m_IsStale = true;
        else if (presented != vk::Result::eSuccess)
            LX_CORE_ERROR("Could not present the frame: {0}", vk::to_string(presented));
    }

    void Swapchain::Create(uint32_t width, uint32_t height)
    {
        const vk::SurfaceCapabilitiesKHR capabilities = m_Context.PhysicalDevice().getSurfaceCapabilitiesKHR(m_Context.Surface());

        // Some surfaces leave the extent to the application and report it as UINT32_MAX.
        m_Extent = capabilities.currentExtent;
        if (m_Extent.width == UINT32_MAX)
            m_Extent = vk::Extent2D(width, height);

        vk::SwapchainCreateInfoKHR swapchainInfo;
        swapchainInfo.surface = m_Context.Surface();
        swapchainInfo.minImageCount = capabilities.minImageCount + (m_Settings.ExtraImage ? 1 : 0);
        if (capabilities.maxImageCount != 0)
            swapchainInfo.minImageCount = std::min(swapchainInfo.minImageCount, capabilities.maxImageCount);
        swapchainInfo.imageFormat = ImageFormat;
        swapchainInfo.imageColorSpace = ImageColorSpace;
        swapchainInfo.imageExtent = m_Extent;
        swapchainInfo.imageArrayLayers = 1;
        // Transfer source lets a screenshot be copied out of a finished frame.
        swapchainInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;
        swapchainInfo.preTransform = capabilities.currentTransform;
        swapchainInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapchainInfo.presentMode = ChoosePresentMode(m_Context.PhysicalDevice(), m_Context.Surface(), m_Settings.VSync);
        swapchainInfo.clipped = VK_TRUE;

        m_Swapchain = m_Context.Device().createSwapchainKHR(swapchainInfo);

        const std::vector<vk::Image> images = m_Context.Device().getSwapchainImagesKHR(m_Swapchain);
        LX_CORE_INFO("Swapchain created: {0}x{1}, {2} images, present mode {3}", m_Extent.width, m_Extent.height, images.size(), vk::to_string(swapchainInfo.presentMode));

        for (vk::Image image : images)
        {
            GpuTextureDesc desc;
            desc.Width = m_Extent.width;
            desc.Height = m_Extent.height;
            desc.Format = ImageFormat;
            desc.Usage = swapchainInfo.imageUsage;
            desc.Name = "SwapchainImage";
            m_Images.push_back(GpuTexture::Wrap(m_Context, image, desc));
            m_RenderFinished.push_back(m_Context.Device().createSemaphore(vk::SemaphoreCreateInfo()));
        }

        m_ImageIndex = 0;
        m_IsStale = false;
    }

    void Swapchain::Destroy()
    {
        // The views of the images have to go before the images do. The GPU is idle, so they can go at once.
        m_Images.clear();
        m_Context.Deletions().Flush();

        for (vk::Semaphore semaphore : m_RenderFinished)
            m_Context.Device().destroySemaphore(semaphore);
        m_RenderFinished.clear();

        m_Context.Device().destroySwapchainKHR(m_Swapchain);
        m_Swapchain = nullptr;
    }
}
