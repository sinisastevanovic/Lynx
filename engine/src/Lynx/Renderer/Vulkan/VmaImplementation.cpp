// The memory allocator is a single header. This file is the one place that compiles its code.
// It finds the Vulkan functions through the pointers it is given when the allocator is created,
// because the engine loads Vulkan at run time and links against no Vulkan library.
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
