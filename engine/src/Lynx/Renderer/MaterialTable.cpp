#include "MaterialTable.h"

#include "Vulkan/UploadQueue.h"
#include "Lynx/Asset/AssetManager.h"
#include "Lynx/Asset/Material.h"
#include "Lynx/Asset/Texture.h"

#include <cstring>
#include <span>

namespace Lynx
{
    namespace
    {
        constexpr size_t SmallestCapacity = 1024;

        // How often the table looks for materials that are gone.
        constexpr uint64_t FramesBetweenSweeps = 256;
    }

    MaterialTable::MaterialTable(GraphicsContext& context, UploadQueue& uploads, AssetManager& assets, const MaterialTextureIndices& defaults)
        : m_Context(context), m_Uploads(uploads), m_Assets(assets), m_Defaults(defaults)
    {
        static_assert(sizeof(GpuMaterial) == 64);
    }

    void MaterialTable::BeginFrame()
    {
        ++m_Frame;
        if (m_Frame % FramesBetweenSweeps != 0)
            return;

        // A place can be handed out again at once: the frames still on the GPU run before the frame
        // that overwrites it.
        std::erase_if(m_Indices, [this](const auto& entry)
        {
            if (!m_Slots[entry.second].Owner.expired())
                return false;

            m_FreeSlots.push_back(entry.second);
            return true;
        });
    }

    uint32_t MaterialTable::IndexOf(const std::shared_ptr<Material>& material)
    {
        const auto [found, isNew] = m_Indices.try_emplace(material.get(), 0u);
        if (isNew)
            found->second = TakeSlot();

        const uint32_t index = found->second;
        Slot& slot = m_Slots[index];

        const bool sameMaterial = !isNew && !slot.Owner.expired() && slot.Owner.lock() == material;
        if (!sameMaterial)
        {
            slot.Owner = material;
            slot.HasTextures = false;
        }

        // A material is given a new version whenever one of its textures is loaded or reloaded,
        // so the lookups in the asset manager are only repeated then.
        if (!slot.HasTextures || slot.Version != material->GetVersion())
        {
            slot.Version = material->GetVersion();
            slot.Textures = Resolve(*material);
            slot.HasTextures = true;
        }

        GpuMaterial current{};
        current.AlbedoColor = material->AlbedoColor;
        current.EmissiveColorStrength = glm::vec4(material->EmissiveColor, material->EmissiveStrength);
        current.Metallic = material->Metallic;
        current.Roughness = material->Roughness;
        current.AlphaCutoff = material->Mode == AlphaMode::Mask ? material->AlphaCutoff : -1.0f;
        current.AlbedoTexture = slot.Textures.Albedo;
        current.NormalTexture = slot.Textures.Normal;
        current.MetallicRoughnessTexture = slot.Textures.MetallicRoughness;
        current.EmissiveTexture = slot.Textures.Emissive;
        current.Sampler = slot.Textures.Sampler;

        // The colours and factors are compared themselves, not by version: the editor changes them
        // directly while a slider is dragged, which gives the material no new version.
        if (!sameMaterial || std::memcmp(&current, &m_Written[index], sizeof(GpuMaterial)) != 0)
            Write(index, current);

        return index;
    }

    uint32_t MaterialTable::TakeSlot()
    {
        if (!m_FreeSlots.empty())
        {
            const uint32_t index = m_FreeSlots.back();
            m_FreeSlots.pop_back();
            return index;
        }

        m_Slots.emplace_back();
        m_Written.emplace_back();
        return static_cast<uint32_t>(m_Slots.size() - 1);
    }

    void MaterialTable::Write(uint32_t index, const GpuMaterial& material)
    {
        m_Written[index] = material;

        if (m_Buffer.Size() >= m_Written.size() * sizeof(GpuMaterial))
        {
            m_Uploads.Upload(m_Buffer.Handle(), index * sizeof(GpuMaterial), std::as_bytes(std::span<const GpuMaterial>(&m_Written[index], 1)));
            return;
        }

        // The table is full. A buffer of twice the size is filled with everything, this material included.
        // Frames still on the GPU keep reading the old buffer, which lives on until they are done.
        size_t capacity = SmallestCapacity;
        while (capacity < m_Written.size())
            capacity *= 2;

        GpuBufferDesc desc;
        desc.Size = capacity * sizeof(GpuMaterial);
        desc.Usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
        desc.Name = "MaterialTable";
        m_Buffer = GpuBuffer(m_Context, desc);
        m_Uploads.Upload(m_Buffer.Handle(), 0, std::as_bytes(std::span<const GpuMaterial>(m_Written)));
    }

    MaterialTextureIndices MaterialTable::Resolve(const Material& material) const
    {
        MaterialTextureIndices indices = m_Defaults;

        const auto loaded = [this](AssetHandle handle) -> std::shared_ptr<Texture>
        {
            if (!handle)
                return nullptr;

            std::shared_ptr<Texture> texture = m_Assets.GetAsset<Texture>(handle);
            return texture && texture->IsOnGpu() ? texture : nullptr;
        };

        // Albedo and emissive are colours, the other two are plain numbers.
        // All textures of a material are read with the sampler of its albedo texture.
        if (const std::shared_ptr<Texture> albedo = loaded(material.AlbedoTexture))
        {
            indices.Albedo = albedo->GetColorTableIndex();
            indices.Sampler = albedo->GetSamplerIndex();
        }
        if (const std::shared_ptr<Texture> normal = loaded(material.NormalMap))
            indices.Normal = normal->GetTableIndex();
        if (const std::shared_ptr<Texture> metallicRoughness = loaded(material.MetallicRoughnessTexture))
            indices.MetallicRoughness = metallicRoughness->GetTableIndex();
        if (const std::shared_ptr<Texture> emissive = loaded(material.EmissiveTexture))
            indices.Emissive = emissive->GetColorTableIndex();

        return indices;
    }
}
