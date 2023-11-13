VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"
IncludeDir["glm"] = "ThirdParty/glm"
IncludeDir["spdlog"] = "ThirdParty/spdlog/include"

LibraryDir = {}
LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"

Library = {}
Library["Vulkan"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"

group "Dependencies"
   include "ThirdParty/imgui"
   include "ThirdParty/glfw"
group ""

group "Core"
    include "Lynx"
group ""