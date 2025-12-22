#include "AssetManager.h"

#include "Asset.h"

namespace Lynx
{
    AssetManager::AssetManager(nvrhi::DeviceHandle device, AssetRegistry* registy)
        : m_Device(device), m_AssetRegistry(registy)
    {
        // TODO: Temporary code! Remove this!
        /*GetDefaultTexture();
        GetWhiteTexture();
        GetErrorTexture();*/
        //GetDefaultCube();
    }

    std::shared_ptr<Asset> AssetManager::GetAsset(AssetHandle handle)
    {
        if (m_LoadedAssets.contains(handle))
            return m_LoadedAssets[handle];

        if (!m_AssetRegistry->Contains(handle))
        {
            LX_CORE_ERROR("AssetManager: Trying to get asset that does not exist ({0})!", handle);
            return nullptr; // Asset does not exist!
        }

        const AssetMetadata& metadata = m_AssetRegistry->Get(handle);

        std::shared_ptr<Asset> asset = LoadAsset(metadata);
        if (asset)
        {
            m_LoadedAssets[handle] = asset;
        }

        return asset;
    }

    std::shared_ptr<Asset> AssetManager::LoadAsset(const AssetMetadata& metadata)
    {
        // TODO: We would replace this with some kind of loaders/importers?
        std::shared_ptr<Asset> newAsset = nullptr;
        switch (metadata.Type)
        {
            case AssetType::Texture:
                newAsset = std::make_shared<Texture>(m_Device, metadata.FilePath.string());
                break;
            case AssetType::StaticMesh:
                newAsset = std::make_shared<StaticMesh>(m_Device, metadata.FilePath.string());
                break;
            case AssetType::None:
            case AssetType::SkeletalMesh:
            case AssetType::Material:
            case AssetType::Shader:
            case AssetType::Scene:
            default: LX_CORE_ERROR("AssetType not supported yet ({0})!", static_cast<int>(metadata.Type)); return nullptr;
        }

        if (newAsset)
            newAsset->SetHandle(metadata.Handle);

        return newAsset;
    }

    std::shared_ptr<Asset> AssetManager::GetAsset(const std::filesystem::path& path)
    {
        AssetHandle handle = m_AssetRegistry->ImportAsset(path);
        return GetAsset(handle);
    }

    AssetHandle AssetManager::GetAssetHandle(const std::filesystem::path& path) const
    {
        if (m_AssetRegistry->Contains(path))
            return m_AssetRegistry->Get(path).Handle;
        return AssetHandle::Null();
    }

    bool AssetManager::IsAssetLoaded(AssetHandle handle) const
    {
        return m_LoadedAssets.contains(handle);
    }

    std::filesystem::path AssetManager::GetAssetPath(AssetHandle handle) const
    {
        if (m_AssetRegistry->Contains(handle))
            return m_AssetRegistry->Get(handle).FilePath;
        return {};
    }

    std::shared_ptr<Texture> AssetManager::GetDefaultTexture()
    {
        auto asset = GetAsset("engine/resources/T_DefaultChecker.png");
        return std::static_pointer_cast<Texture>(asset);
        /*if (m_AssetPaths.contains("DEFAULT_TEXTURE"))
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

        return tex;*/
    }

    std::shared_ptr<Texture> AssetManager::GetWhiteTexture()
    {
        auto asset = GetAsset("engine/resources/T_White.png");
        return std::static_pointer_cast<Texture>(asset);
        /*if (m_AssetPaths.contains("WHITE_TEXTURE"))
            return GetAsset<Texture>(m_AssetPaths["WHITE_TEXTURE"]);

        const int w = 1;
        const int h = 1;
        std::vector<uint8_t> data = { 255, 255, 255, 255 };

        auto tex = std::make_shared<Texture>(m_Device, data, w, h, "WhiteTexture");
        m_Assets[tex->GetHandle()] = tex;
        m_AssetPaths["WHITE_TEXTURE"] = tex->GetHandle();

        return tex;*/
    }

    std::shared_ptr<Texture> AssetManager::GetErrorTexture()
    {
        auto asset = GetAsset("engine/resources/T_Error.png");
        return std::static_pointer_cast<Texture>(asset);
        /*if (m_AssetPaths.contains("ERROR_TEXTURE"))
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

        return tex;*/
    }


    std::shared_ptr<StaticMesh> AssetManager::GetDefaultCube()
    {
        auto asset = GetAsset("engine/resources/SM_DefaultCube.gltf");
        return std::static_pointer_cast<StaticMesh>(asset);
        /*if (m_AssetPaths.contains("DEFAULT_CUBE"))
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

        return mesh;*/
    }
}
