file(GLOB_RECURSE ENGINE_SOURCES
    CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.h"
)

add_library(engine SHARED ${ENGINE_SOURCES})

target_compile_definitions(engine PRIVATE 
    LX_BUILD_DLL
    GLFW_STATIC
)

target_include_directories(engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_include_directories(engine
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_precompile_headers(engine PRIVATE src/lxpch.h)

find_package(entt CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(unofficial-shaderc CONFIG REQUIRED)

target_link_libraries(engine PUBLIC

    nvrhi

    nvrhi_vk

    EnTT::EnTT

    glfw

    glm::glm

    spdlog::spdlog

    Vulkan::Vulkan

    unofficial::shaderc::shaderc

)
