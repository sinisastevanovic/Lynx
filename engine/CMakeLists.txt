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
)

FetchContent_GetProperties(nvrhi)
if(NOT nvrhi_POPULATED)
    FetchContent_Populate(nvrhi)
endif()

set(NVRHI_INTERNAL_VK_HEADERS "${nvrhi_SOURCE_DIR}/thirdparty/Vulkan-Headers/include")

target_include_directories(engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_include_directories(engine
    BEFORE PRIVATE
        "${NVRHI_INTERNAL_VK_HEADERS}"
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_precompile_headers(engine PRIVATE src/lxpch.h)

find_package(entt CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(unofficial-shaderc CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(engine PUBLIC

    nvrhi

    nvrhi_vk

    EnTT::EnTT

    glfw

    glm::glm

    spdlog::spdlog

    unofficial::shaderc::shaderc

    Jolt::Jolt

    nlohmann_json::nlohmann_json

    ${LUA_LIBRARIES}
)
