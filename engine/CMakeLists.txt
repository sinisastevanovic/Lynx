file(GLOB_RECURSE ENGINE_SOURCES
    CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.h"
)

add_library(engine SHARED ${ENGINE_SOURCES})

# Engine headers need C++23, so everything that includes them does too.
target_compile_features(engine PUBLIC cxx_std_23)

target_compile_definitions(engine
    PRIVATE
        LX_BUILD_DLL
        GLFW_STATIC
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        # The engine does not link the Vulkan library, so ImGui's Vulkan backend loads its functions at run time.
        IMGUI_IMPL_VULKAN_NO_PROTOTYPES
        "IMGUI_API=__declspec(dllexport)"
        "LX_ENGINE_RESOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/resources\""
    # Everything that uses the engine must see the same GLM depth range.
    PUBLIC
        GLM_FORCE_DEPTH_ZERO_TO_ONE
    # ImGui is compiled into the engine DLL, so users of the engine import it from there.
    INTERFACE
        "IMGUI_API=__declspec(dllimport)"
)

find_package(Lua REQUIRED)

target_include_directories(engine
    PUBLIC
        ${imgui_SOURCE_DIR}
    PRIVATE
        ${imgui_SOURCE_DIR}/backends
        ${LUA_INCLUDE_DIR}
)

target_sources(engine PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
)

target_sources(engine PRIVATE
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_include_directories(engine
    BEFORE PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_precompile_headers(engine PRIVATE src/lxpch.h)

find_package(entt CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

# Only the headers: the engine loads the Vulkan library itself at run time, through GLFW.
find_package(VulkanHeaders CONFIG REQUIRED)
find_package(slang CONFIG REQUIRED)

target_link_libraries(engine PUBLIC


    EnTT::EnTT

    glfw

    glm::glm

    spdlog::spdlog

    Jolt::Jolt

    nlohmann_json::nlohmann_json

    GPUOpen::VulkanMemoryAllocator

    ${LUA_LIBRARIES}
)

# The Slang shader compiler. Only the engine uses it, and it needs its own files next to the engine at run time:
# the compiler, the SPIR-V optimizer it loads on demand, and its standard modules.
target_link_libraries(engine PRIVATE slang::slang Vulkan::Headers)

set(LYNX_SLANG_RUNTIME_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<$<CONFIG:Debug>:debug/>bin")
file(GLOB LYNX_SLANG_STANDARD_MODULES RELATIVE "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/slang-standard-module-*")

add_custom_command(TARGET engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LYNX_SLANG_RUNTIME_DIR}/slang.dll"
        "${LYNX_SLANG_RUNTIME_DIR}/slang-compiler.dll"
        "${LYNX_SLANG_RUNTIME_DIR}/slang-glslang.dll"
        "${LYNX_SLANG_RUNTIME_DIR}/slang.slang"
        "$<TARGET_FILE_DIR:engine>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${LYNX_SLANG_RUNTIME_DIR}/${LYNX_SLANG_STANDARD_MODULES}"
        "$<TARGET_FILE_DIR:engine>/${LYNX_SLANG_STANDARD_MODULES}"
    VERBATIM
)
