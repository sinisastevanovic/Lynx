#pragma once

#include <array>
#include "Vulkan/GpuBuffer.h"

#include <glm/glm.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace Lynx
{
    class AssetManager;
    class GraphicsContext;
    class Material;
    class UploadQueue;

    // Where the texture and sampler tables have the textures of a material.
    struct MaterialTextureIndices
    {
        uint32_t Albedo = 0;
        uint32_t Normal = 0;
        uint32_t MetallicRoughness = 0;
        uint32_t Emissive = 0;
        uint32_t Sampler = 0;

        // What a cut-out is tested from, see Texture::GetCutOutTableIndex.
        uint32_t CutOut = 0;
    };

    // Every material that is drawn, as one buffer on the GPU. An instance carries the index of its material
    // and the shader reads the material from here, so a draw call no longer has to be about one material.
    //
    // A material is written to the buffer when it first appears and again only when it has changed.
    class MaterialTable
    {
    public:
        // The defaults stand in for a texture a material does not have, or one that is still loading.
        MaterialTable(GraphicsContext& context, UploadQueue& uploads, AssetManager& assets, const MaterialTextureIndices& defaults);

        // Call at the start of every frame. Now and then, the places of materials that are gone become free.
        void BeginFrame();

        // The place of a material in the table. Call it in every frame the material is drawn in, before
        // the frame's uploads are recorded: this is also where a change to the material is noticed.
        uint32_t IndexOf(const std::shared_ptr<Material>& material);

        // The address shaders read the table from. It changes when the table grows, so ask every frame,
        // after the last IndexOf.
        uint64_t Address() const { return m_Buffer.Address(); }

    private:
        // A material as shaders read it. Has to match StandardMaterial in Material.slang.
        struct GpuMaterial
        {
            glm::vec4 AlbedoColor;

            // The colour in rgb, the strength in a.
            glm::vec4 EmissiveColorStrength;
            float Metallic;
            float Roughness;

            // Negative for a material without cut-outs.
            float AlphaCutoff;
            uint32_t AlbedoTexture;
            uint32_t NormalTexture;
            uint32_t MetallicRoughnessTexture;
            uint32_t EmissiveTexture;
            uint32_t Sampler;
            uint32_t CutOutTexture;

            // Keeps the size a multiple of 16 bytes, which a shader takes it to be.
            std::array<uint32_t, 3> Padding;
        };

        struct Slot
        {
            // Tells a material apart from a later one that happens to live at the same address.
            std::weak_ptr<Material> Owner;

            // The version the textures were looked up for.
            uint32_t Version = 0;
            bool HasTextures = false;
            MaterialTextureIndices Textures;
        };

        MaterialTextureIndices Resolve(const Material& material) const;
        uint32_t TakeSlot();
        void Write(uint32_t index, const GpuMaterial& material);

    private:
        GraphicsContext& m_Context;
        UploadQueue& m_Uploads;
        AssetManager& m_Assets;
        MaterialTextureIndices m_Defaults;

        GpuBuffer m_Buffer;

        // What the buffer holds, kept to compare against and to fill a larger buffer from.
        std::vector<GpuMaterial> m_Written;
        std::vector<Slot> m_Slots;
        std::vector<uint32_t> m_FreeSlots;
        std::unordered_map<const Material*, uint32_t> m_Indices;
        uint64_t m_Frame = 0;
    };
}
