#include "AssetManager.h"

#include "Asset.h"

namespace Lynx
{
    AssetManager::AssetManager(nvrhi::DeviceHandle device)
        : m_Device(device)
    {
        // TODO: Temporary code! Remove this!
        GetDefaultTexture();
        GetWhiteTexture();
        GetErrorTexture();
        //GetDefaultCube();
    }

    std::shared_ptr<Texture> AssetManager::GetTexture(const std::string& filepath)
    {
        if (m_AssetPaths.contains(filepath))
            return GetAsset<Texture>(m_AssetPaths[filepath]);

        std::shared_ptr<Texture> newTexture = std::make_shared<Texture>(m_Device, filepath);
        if (!newTexture->GetHandle().IsValid())
        {
            return GetErrorTexture();
        }
        m_Assets[newTexture->GetHandle()] = newTexture;
        m_AssetPaths[filepath] = newTexture->GetHandle();

        LX_CORE_INFO("Loaded Texture: {0} (UUID: {1})", filepath, newTexture->GetHandle());
        return newTexture;
    }

    std::shared_ptr<Texture> AssetManager::GetDefaultTexture()
    {
        if (m_AssetPaths.contains("DEFAULT_TEXTURE"))
            return GetAsset<Texture>(m_AssetPaths["DEFAULT_TEXTURE"]);

        const int w = 64;
        const int h = 64;
        std::vector<uint8_t> data(w * h * 4);

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                bool isDark = (x / 8 + y / 8) % 2 == 0;
                uint8_t c = isDark ? 128 : 255;

                int i = (y * w + x) * 4;
                data[i + 0] = c;
                data[i + 1] = c;
                data[i + 2] = c;
                data[i + 3] = 255;
            }
        }

        auto tex = std::make_shared<Texture>(m_Device, data, w, h, "DefaultTexture");
        m_Assets[tex->GetHandle()] = tex;
        m_AssetPaths["DEFAULT_TEXTURE"] = tex->GetHandle();

        return tex;
    }

    std::shared_ptr<Texture> AssetManager::GetWhiteTexture()
    {
        if (m_AssetPaths.contains("WHITE_TEXTURE"))
            return GetAsset<Texture>(m_AssetPaths["WHITE_TEXTURE"]);

        const int w = 1;
        const int h = 1;
        std::vector<uint8_t> data = { 255, 255, 255, 255 };

        auto tex = std::make_shared<Texture>(m_Device, data, w, h, "WhiteTexture");
        m_Assets[tex->GetHandle()] = tex;
        m_AssetPaths["WHITE_TEXTURE"] = tex->GetHandle();

        return tex;
    }

    std::shared_ptr<Texture> AssetManager::GetErrorTexture()
    {
        if (m_AssetPaths.contains("ERROR_TEXTURE"))
            return GetAsset<Texture>(m_AssetPaths["ERROR_TEXTURE"]);

        const int w = 64;
        const int h = 64;
        std::vector<uint8_t> data(w * h * 4);

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                bool isDark = (x / 8 + y / 8) % 2 == 0;
                uint8_t c = isDark ? 128 : 255;

                int i = (y * w + x) * 4;
                data[i + 0] = c;
                data[i + 1] = 0;
                data[i + 2] = c;
                data[i + 3] = 255;
            }
        }

        auto tex = std::make_shared<Texture>(m_Device, data, w, h, "ErrorTexture");
        m_Assets[tex->GetHandle()] = tex;
        m_AssetPaths["ERROR_TEXTURE"] = tex->GetHandle();

        return tex;
    }

    std::shared_ptr<StaticMesh> AssetManager::GetMesh(const std::string& filepath)
    {
        if (m_AssetPaths.contains(filepath))
            return GetAsset<StaticMesh>(m_AssetPaths[filepath]);

        std::shared_ptr<StaticMesh> newMesh = std::make_shared<StaticMesh>(m_Device, filepath);
        m_Assets[newMesh->GetHandle()] = newMesh;
        m_AssetPaths[filepath] = newMesh->GetHandle();

        LX_CORE_INFO("Loaded Mesh: {0} (UUID: {1})", filepath, newMesh->GetHandle());
        return newMesh;
    }

    std::shared_ptr<StaticMesh> AssetManager::GetDefaultCube()
    {
        if (m_AssetPaths.contains("DEFAULT_CUBE"))
            return GetAsset<StaticMesh>(m_AssetPaths["DEFAULT_CUBE"]);

        std::vector<Vertex> vertices = {
            // Front face (Z = 0.5)
            { {-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f} },
            { { 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f} },
            { { 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f} },
            { {-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f} },
            // Back face (Z = -0.5)
            { {-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f} },
            { {-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f} },
            { { 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f} },
            { { 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f} },
            // Top face (Y = 0.5)
            { {-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f} },
            { {-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f} },
            { { 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f} },
            { { 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f} },
            // Bottom face (Y = -0.5)
            { {-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f} },
            { { 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f} },
            { { 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f} },
            { {-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f} },
            // Right face (X = 0.5)
            { { 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f} },
            { { 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f} },
            { { 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f} },
            { { 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f} },
            // Left face (X = -0.5)
            { {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f} },
            { {-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f} },
            { {-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f} },
            { {-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f} }
        };

        std::vector<uint32_t> indices = {
            0, 1, 2,  2, 3, 0,       // Front
            4, 5, 6,  6, 7, 4,       // Back
            8, 9, 10, 10, 11, 8,     // Top
            12, 13, 14, 14, 15, 12,  // Bottom
            16, 17, 18, 18, 19, 16,  // Right
            20, 21, 22, 22, 23, 20   // Left
        };

        auto mesh = std::make_shared<StaticMesh>(m_Device, vertices, indices);
        m_Assets[mesh->GetHandle()] = mesh;
        m_AssetPaths["DEFAULT_CUBE"] = mesh->GetHandle();

        return mesh;
    }

    std::shared_ptr<Asset> AssetManager::GetAsset(AssetHandle handle)
    {
        if (m_Assets.contains(handle))
        {
            return m_Assets[handle];
        }
        return nullptr;
    }

    bool AssetManager::IsAssetLoaded(AssetHandle handle) const
    {
        return m_Assets.contains(handle);
    }

    std::string AssetManager::GetAssetPath(AssetHandle handle)
    {
        for (auto& [path, cacheHandle] : m_AssetPaths)
        {
            if (cacheHandle == handle)
                return path;
        }

        return "";
    }

    void AssetManager::AddAssetToCache(const std::string& filepath, std::shared_ptr<Asset> asset)
    {
    }
}
