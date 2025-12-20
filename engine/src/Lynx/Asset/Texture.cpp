#include "lxpch.h"
#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Lynx
{
    Texture::Texture(nvrhi::DeviceHandle device, const std::string& filepath)
    {
        // stbi_set_flip_vertically_on_load(1);
        if (!std::filesystem::exists(filepath))
        {
            LX_CORE_ERROR("File does not exist: {0}", filepath);
            return;
        }
        
        int width, height, channels;
        stbi_uc* data = stbi_load(filepath.c_str(), &width, &height, &channels, 4);

        if (!data)
        {
            LX_CORE_ERROR("Failed to load texture: {0}", filepath);
            // TODO: Load "Missing Texture" texture here
            return;
        }

        m_Width = width;
        m_Height = height;

        // Create NVRHI Texture description
        auto desc = nvrhi::TextureDesc()
            .setDimension(nvrhi::TextureDimension::Texture2D)
            .setWidth(width)
            .setHeight(height)
            .setFormat(nvrhi::Format::SRGBA8_UNORM)
            .enableAutomaticStateTracking(nvrhi::ResourceStates::ShaderResource)
            .setDebugName(filepath);

        m_TextureHandle = device->createTexture(desc);
        if (!m_TextureHandle)
        {
            LX_CORE_ERROR("Failed to create NVRHI texture: {0}", filepath);
            stbi_image_free(data);
            return;
        }

        // Upload data to GPU with temporary Command List
        // TODO: Later we should batch these uploads in the AssetManager...
        nvrhi::CommandListHandle commandList = device->createCommandList();
        commandList->open();

        size_t rowPitch = width * 4;
        size_t depthPitch = 0;

        commandList->writeTexture(m_TextureHandle, 0, 0, data, rowPitch, depthPitch);
        commandList->close();
        device->executeCommandList(commandList);

        stbi_image_free(data);
        LX_CORE_TRACE("Texture loaded successfully: {0} ({1}x{2})", filepath, width, height);
    }
}
