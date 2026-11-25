#include "DeviceSelection.h"

#include "Lynx/Log.h"

#include <algorithm>
#include <optional>
#include <string_view>

namespace Lynx
{
    namespace
    {
        // The feature structs a device is queried and created with, linked into one chain.
        // The structs point at each other, so a chain is filled in place and never copied.
        struct FeatureChain
        {
            vk::PhysicalDeviceFeatures2 Core;
            vk::PhysicalDeviceVulkan11Features Vulkan11;
            vk::PhysicalDeviceVulkan12Features Vulkan12;
            vk::PhysicalDeviceVulkan13Features Vulkan13;
            vk::PhysicalDeviceMeshShaderFeaturesEXT MeshShader;

            // A struct that belongs to an extension may only be in the chain if the GPU has that extension.
            explicit FeatureChain(bool includeMeshShader)
            {
                Core.pNext = &Vulkan11;
                Vulkan11.pNext = &Vulkan12;
                Vulkan12.pNext = &Vulkan13;
                Vulkan13.pNext = includeMeshShader ? &MeshShader : nullptr;
            }

            FeatureChain(const FeatureChain&) = delete;
            FeatureChain& operator=(const FeatureChain&) = delete;
        };

        struct Feature
        {
            const char* Name;
            vk::Bool32& (*Get)(FeatureChain&);
        };

#define LX_FEATURE(member, field) Feature{ #field, [](FeatureChain& chain) -> vk::Bool32& { return chain.member.field; } }

        // Without these the renderer does not run.
        const Feature RequiredFeatures[] = {
            LX_FEATURE(Core.features, independentBlend),
            LX_FEATURE(Core.features, samplerAnisotropy),
            // Drawing triangles as their edges, for the wireframe view.
            LX_FEATURE(Core.features, fillModeNonSolid),
            // Drawing many objects with one call whose arguments the GPU wrote itself.
            LX_FEATURE(Core.features, multiDrawIndirect),
            LX_FEATURE(Core.features, drawIndirectFirstInstance),
            LX_FEATURE(Vulkan12, drawIndirectCount),
            LX_FEATURE(Vulkan11, shaderDrawParameters),
            LX_FEATURE(Core.features, shaderInt64),
            // One table of all textures, indexed in the shader.
            LX_FEATURE(Vulkan12, descriptorIndexing),
            LX_FEATURE(Vulkan12, runtimeDescriptorArray),
            LX_FEATURE(Core.features, shaderSampledImageArrayDynamicIndexing),
            LX_FEATURE(Vulkan12, shaderSampledImageArrayNonUniformIndexing),
            LX_FEATURE(Vulkan12, descriptorBindingPartiallyBound),
            LX_FEATURE(Vulkan12, descriptorBindingVariableDescriptorCount),
            LX_FEATURE(Vulkan12, descriptorBindingSampledImageUpdateAfterBind),
            // A table of pictures that compute shaders write into, whatever the format of each.
            LX_FEATURE(Vulkan12, descriptorBindingStorageImageUpdateAfterBind),
            LX_FEATURE(Core.features, shaderStorageImageArrayDynamicIndexing),
            LX_FEATURE(Core.features, shaderStorageImageWriteWithoutFormat),
            LX_FEATURE(Core.features, shaderStorageImageReadWithoutFormat),
            LX_FEATURE(Vulkan12, descriptorBindingUpdateUnusedWhilePending),
            LX_FEATURE(Vulkan12, scalarBlockLayout),
            LX_FEATURE(Vulkan12, timelineSemaphore),
            LX_FEATURE(Vulkan12, bufferDeviceAddress),
            // The GPU timers reset their queries from the CPU.
            LX_FEATURE(Vulkan12, hostQueryReset),
            LX_FEATURE(Vulkan13, dynamicRendering),
            LX_FEATURE(Vulkan13, synchronization2),
            LX_FEATURE(Vulkan13, maintenance4),
        };

        // Enabled where the GPU has them.
        const Feature MinMaxSamplerFilter = LX_FEATURE(Vulkan12, samplerFilterMinmax);
        const Feature SmallIntegerStorageFeatures[] = {
            LX_FEATURE(Vulkan11, storageBuffer16BitAccess),
            LX_FEATURE(Vulkan12, storageBuffer8BitAccess),
            LX_FEATURE(Vulkan12, shaderInt8),
            LX_FEATURE(Core.features, shaderInt16),
        };
        const Feature MeshShaderFeatures[] = {
            LX_FEATURE(MeshShader, meshShader),
            LX_FEATURE(MeshShader, taskShader),
        };

#undef LX_FEATURE

        template<size_t Count>
        bool HasAll(FeatureChain& supported, const Feature (&features)[Count])
        {
            return std::ranges::all_of(features, [&supported](const Feature& feature) { return feature.Get(supported) == VK_TRUE; });
        }

        template<size_t Count>
        void EnableAll(FeatureChain& enabled, const Feature (&features)[Count])
        {
            for (const Feature& feature : features)
                feature.Get(enabled) = VK_TRUE;
        }

        bool HasExtension(const std::vector<vk::ExtensionProperties>& extensions, std::string_view name)
        {
            return std::ranges::any_of(extensions, [name](const vk::ExtensionProperties& extension)
            {
                return std::string_view(extension.extensionName.data()) == name;
            });
        }

        std::optional<uint32_t> FindGraphicsQueueFamily(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface)
        {
            const std::vector<vk::QueueFamilyProperties> families = physicalDevice.getQueueFamilyProperties();
            for (uint32_t family = 0; family < families.size(); ++family)
            {
                const bool canDraw = static_cast<bool>(families[family].queueFlags & vk::QueueFlagBits::eGraphics);
                if (canDraw && physicalDevice.getSurfaceSupportKHR(family, surface) == VK_TRUE)
                    return family;
            }
            return std::nullopt;
        }

        struct Candidate
        {
            vk::PhysicalDevice PhysicalDevice;
            std::string Name;
            uint32_t GraphicsQueueFamily = 0;
            int Score = 0;

            // Why the GPU cannot be used. Empty if it can.
            std::string Problem;
        };

        Candidate Examine(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface)
        {
            const vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();

            Candidate candidate;
            candidate.PhysicalDevice = physicalDevice;
            candidate.Name = properties.deviceName.data();

            if (properties.apiVersion < VK_API_VERSION_1_3)
            {
                candidate.Problem = "its driver does not support Vulkan 1.3";
                return candidate;
            }

            const std::vector<vk::ExtensionProperties> extensions = physicalDevice.enumerateDeviceExtensionProperties();
            if (!HasExtension(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
            {
                candidate.Problem = "it cannot present pictures to a window";
                return candidate;
            }

            const std::optional<uint32_t> queueFamily = FindGraphicsQueueFamily(physicalDevice, surface);
            if (!queueFamily)
            {
                candidate.Problem = "it has no queue that can both draw and present to this window";
                return candidate;
            }
            candidate.GraphicsQueueFamily = *queueFamily;

            FeatureChain supported(HasExtension(extensions, VK_EXT_MESH_SHADER_EXTENSION_NAME));
            physicalDevice.getFeatures2(&supported.Core);

            for (const Feature& feature : RequiredFeatures)
            {
                if (feature.Get(supported) != VK_TRUE)
                    candidate.Problem += (candidate.Problem.empty() ? "it lacks the features " : ", ") + std::string(feature.Name);
            }
            if (!candidate.Problem.empty())
                return candidate;

            // A dedicated graphics card beats a GPU built into the processor, which beats anything else.
            switch (properties.deviceType)
            {
                case vk::PhysicalDeviceType::eDiscreteGpu:   candidate.Score = 3; break;
                case vk::PhysicalDeviceType::eIntegratedGpu: candidate.Score = 2; break;
                default:                                     candidate.Score = 1; break;
            }
            return candidate;
        }
    }

    std::expected<GraphicsDevice, std::string> CreateGraphicsDevice(vk::Instance instance, vk::SurfaceKHR surface)
    {
        std::vector<Candidate> candidates;
        for (vk::PhysicalDevice physicalDevice : instance.enumeratePhysicalDevices())
            candidates.push_back(Examine(physicalDevice, surface));

        // The first of the best ones: stable_sort keeps the driver's order among equals.
        std::ranges::stable_sort(candidates, std::greater<>(), &Candidate::Score);

        if (candidates.empty() || candidates.front().Score == 0)
        {
            std::string message = "No GPU in this machine can run the renderer.";
            for (const Candidate& candidate : candidates)
                message += " " + candidate.Name + ": " + candidate.Problem + ".";
            return std::unexpected(message);
        }

        const Candidate& chosen = candidates.front();
        for (const Candidate& candidate : candidates)
        {
            if (&candidate != &chosen && !candidate.Problem.empty())
                LX_CORE_INFO("GPU '{0}' was not considered: {1}.", candidate.Name, candidate.Problem);
        }

        GraphicsDevice result;
        result.PhysicalDevice = chosen.PhysicalDevice;
        result.GraphicsQueueFamily = chosen.GraphicsQueueFamily;
        result.Extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        const std::vector<vk::ExtensionProperties> extensions = chosen.PhysicalDevice.enumerateDeviceExtensionProperties();
        const bool hasMeshShaderExtension = HasExtension(extensions, VK_EXT_MESH_SHADER_EXTENSION_NAME);

        FeatureChain supported(hasMeshShaderExtension);
        chosen.PhysicalDevice.getFeatures2(&supported.Core);

        result.HasMemoryBudget = HasExtension(extensions, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
        result.HasMeshShaders = hasMeshShaderExtension && HasAll(supported, MeshShaderFeatures);
        result.HasMinMaxSamplerFilter = MinMaxSamplerFilter.Get(supported) == VK_TRUE;
        result.HasSmallIntegerStorage = HasAll(supported, SmallIntegerStorageFeatures);

        FeatureChain enabled(result.HasMeshShaders);
        EnableAll(enabled, RequiredFeatures);

        if (result.HasMemoryBudget)
            result.Extensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
        if (result.HasMeshShaders)
        {
            result.Extensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
            EnableAll(enabled, MeshShaderFeatures);
        }
        if (result.HasMinMaxSamplerFilter)
            MinMaxSamplerFilter.Get(enabled) = VK_TRUE;
        if (result.HasSmallIntegerStorage)
            EnableAll(enabled, SmallIntegerStorageFeatures);

        const float queuePriority = 1.0f;
        vk::DeviceQueueCreateInfo queueInfo;
        queueInfo.queueFamilyIndex = result.GraphicsQueueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        vk::DeviceCreateInfo deviceInfo;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = static_cast<uint32_t>(result.Extensions.size());
        deviceInfo.ppEnabledExtensionNames = result.Extensions.data();
        deviceInfo.pNext = &enabled.Core;
        result.Device = chosen.PhysicalDevice.createDevice(deviceInfo);

        LX_CORE_INFO("Rendering on '{0}'. Mesh shaders: {1}. Min/max sampler filter: {2}. 8-bit and 16-bit storage: {3}. Memory budget: {4}.",
            chosen.Name, result.HasMeshShaders, result.HasMinMaxSamplerFilter, result.HasSmallIntegerStorage, result.HasMemoryBudget);
        return result;
    }
}
