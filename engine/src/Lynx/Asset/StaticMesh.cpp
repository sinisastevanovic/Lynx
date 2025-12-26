#include "StaticMesh.h"
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>
#include <filesystem>

#include "Lynx/Engine.h"

namespace Lynx
{
    StaticMesh::StaticMesh(const std::string& filepath)
        : m_FilePath(filepath)
    {
        LoadAndUpload();
    }

    StaticMesh::StaticMesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices)
    {
        auto submeshMaterial = std::make_shared<Material>();
        auto defaultTex = Engine::Get().GetAssetManager().GetDefaultTexture();
        if (defaultTex)
            submeshMaterial->AlbedoTexture = defaultTex->GetHandle();
        
        auto [vb, ib] = Engine::Get().GetRenderer().CreateMeshBuffers(vertices, indices);
        Submesh sub;
        sub.VertexBuffer = vb;
        sub.IndexBuffer = ib;
        sub.IndexCount = (uint32_t)indices.size();
        sub.Material = submeshMaterial;
        sub.Name = "RuntimeCreated";
    }

    bool StaticMesh::Reload()
    {
        if (m_FilePath.empty())
            return false;

        LoadAndUpload();

        return true;
    }

    void StaticMesh::LoadAndUpload()
    {
        m_Submeshes.clear();

        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, m_FilePath);
        if (!ret)
        {
            ret = loader.LoadBinaryFromFile(&model, &err, &warn, m_FilePath);
        }

        if (!ret)
        {
            LX_CORE_ERROR("Failed to load glTF: {0}", err);
            return;
        }

        for (const auto& mesh : model.meshes)
        {
            for (const auto& primitive : mesh.primitives)
            {
                const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
                const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
                const unsigned char* posData = &model.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset];
                int posStride = posAccessor.ByteStride(posView);

                // Get TexCoords
                const tinygltf::Accessor& uvAccessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
                const unsigned char* uvData = &model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset];
                int uvStride = uvAccessor.ByteStride(uvView);

                const float* normalsBuffer = nullptr;
                int normalStride = 0;
                if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
                {
                    const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("NORMAL")];
                    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                    normalsBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    normalStride = accessor.ByteStride(view);
                }

                

                const float* tangentsBuffer = nullptr;
                int tangentStride = 0;
                if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
                {
                    const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TANGENT")];
                    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
                    {
                        LX_CORE_ERROR("Tanget format not supported yet! (Expected FLOAT)");
                        tangentsBuffer = nullptr;
                    }
                    else
                    {
                        const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                        tangentsBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                        tangentStride = accessor.ByteStride(view);
                    }
                }

                std::vector<Vertex> vertices(posAccessor.count);
                for (size_t i = 0; i < posAccessor.count; ++i)
                {
                    const float* p = reinterpret_cast<const float*>(posData + i * posStride);
                    vertices[i].Position = {p[0], p[1], p[2]};

                    if (normalsBuffer)
                    {
                        const float* n = reinterpret_cast<const float*>((const byte*)normalsBuffer + i * normalStride);
                        vertices[i].Normal = {n[0], n[1], n[2]};
                    }
                    else
                    {
                        vertices[i].Normal = { 0.0f, 1.0f, 0.0f };
                    }

                    if (tangentsBuffer)
                    {
                        const float* t = reinterpret_cast<const float*>((const byte*)tangentsBuffer + i * tangentStride);
                        vertices[i].Tangent = {t[0], t[1], t[2], t[3]};
                    }
                    else
                    {
                        vertices[i].Tangent = { 0.0f, 0.0f, 0.0f, 1.0f };
                    }

                    if (uvData)
                    {
                        const float* t = reinterpret_cast<const float*>(uvData + i * uvStride);
                        vertices[i].TexCoord = {t[0], t[1]};
                    }
                    else
                    {
                        vertices[i].TexCoord = { 0.0f, 0.0f };
                    }
                }

                // Get indices
                std::vector<uint32_t> indices;
                if (primitive.indices >= 0)
                {
                    const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
                    const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
                    const unsigned char* indexPtr = &model.buffers[indexView.buffer].data[indexAccessor.byteOffset + indexView.byteOffset];

                    indices.reserve(indexAccessor.count);

                    if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        const uint16_t* ptr = reinterpret_cast<const uint16_t*>(indexPtr);
                        for (size_t i = 0; i < indexAccessor.count; ++i) indices.push_back(ptr[i]);
                    }
                    else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                    {
                        const uint32_t* ptr = reinterpret_cast<const uint32_t*>(indexPtr);
                        for (size_t i = 0; i < indexAccessor.count; ++i) indices.push_back(ptr[i]);
                    }
                    else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                    {
                        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(indexPtr);
                        for (size_t i = 0; i < indexAccessor.count; ++i) indices.push_back(ptr[i]);
                    }
                }
                else
                {
                    indices.reserve(posAccessor.count);
                    for (size_t i = 0; i < posAccessor.count; ++i)
                    {
                        indices.push_back((uint32_t)i);
                    }
                }

                // Generate missing normals
                if (!normalsBuffer)
                {
                    for (size_t i = 0; i < indices.size(); i += 3)
                    {
                        uint32_t idx0 = indices[i+0];
                        uint32_t idx1 = indices[i+1];
                        uint32_t idx2 = indices[i+2];

                        glm::vec3 v0 = vertices[idx0].Position;
                        glm::vec3 v1 = vertices[idx1].Position;
                        glm::vec3 v2 = vertices[idx2].Position;

                        glm::vec3 edge1 = v1 - v0;
                        glm::vec3 edge2 = v2 - v0;
                        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

                        vertices[idx0].Normal += normal;
                        vertices[idx1].Normal += normal;
                        vertices[idx2].Normal += normal;
                    }

                    for (auto& v : vertices)
                    {
                        if (glm::length(v.Normal) > 0.0f)
                            v.Normal = glm::normalize(v.Normal);
                    }
                }

                if (!tangentsBuffer)
                {
                    for (size_t i = 0; i < indices.size(); i += 3)
                    {
                        uint32_t idx0 = indices[i];
                        uint32_t idx1 = indices[i+1];
                        uint32_t idx2 = indices[i+2];

                        auto& v0 = vertices[idx0];
                        auto& v1 = vertices[idx1];
                        auto& v2 = vertices[idx2];

                        glm::vec3 edge1 = v1.Position - v0.Position;
                        glm::vec3 edge2 = v2.Position - v0.Position;

                        glm::vec2 deltaUV1 = v1.TexCoord - v0.TexCoord;
                        glm::vec2 deltaUV2 = v2.TexCoord - v0.TexCoord;

                        float det = (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
                        if (abs(det) < 0.0001f) continue;
                        float f = 1.0f / det;

                        glm::vec3 tangent;
                        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
                        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
                        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

                        v0.Tangent += glm::vec4(tangent, 0.0f);
                        v1.Tangent += glm::vec4(tangent, 0.0f);
                        v2.Tangent += glm::vec4(tangent, 0.0f);
                    }

                    for (auto& v : vertices)
                    {
                        if (glm::length(glm::vec3(v.Tangent)) > 0.0f)
                            v.Tangent = glm::vec4(glm::normalize(glm::vec3(v.Tangent)), 1.0f);
                        else
                            v.Tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                    }
                }
                

                // TODO: we should probably create a new material with the asset system here later.
                auto submeshMaterial = std::make_shared<Material>();
                if (primitive.material >= 0)
                {
                    const tinygltf::Material& gltfMaterial = model.materials[primitive.material];
                    const auto& pbr = gltfMaterial.pbrMetallicRoughness;

                    submeshMaterial->AlbedoColor = glm::vec4(
                        (float)pbr.baseColorFactor[0],
                        (float)pbr.baseColorFactor[1],
                        (float)pbr.baseColorFactor[2],
                        (float)pbr.baseColorFactor[3]
                    );

                    if (pbr.baseColorTexture.index >= 0)
                    {
                        int imageIndex = model.textures[pbr.baseColorTexture.index].source;
                        std::string texturePath = std::filesystem::path(m_FilePath).parent_path().string() + "/" +
                            model.images[imageIndex].uri;
                        submeshMaterial->AlbedoTexture = Engine::Get().GetAssetManager().GetAssetHandle(texturePath);
                    }

                    submeshMaterial->Metallic = (float)pbr.metallicFactor;
                    submeshMaterial->Roughness = (float)pbr.roughnessFactor;

                    if (pbr.metallicRoughnessTexture.index >= 0)
                    {
                        int imageIndex = model.textures[pbr.metallicRoughnessTexture.index].source;
                        std::string texturePath = std::filesystem::path(m_FilePath).parent_path().string() + "/" +
                            model.images[imageIndex].uri;
                        submeshMaterial->MetallicRoughnessTexture = Engine::Get().GetAssetManager().GetAssetHandle(texturePath);
                    }

                    if (gltfMaterial.normalTexture.index >= 0)
                    {
                        int imageIndex = model.textures[gltfMaterial.normalTexture.index].source;
                        std::string texturePath = std::filesystem::path(m_FilePath).parent_path().string() + "/" +
                            model.images[imageIndex].uri;
                        submeshMaterial->NormalMap = Engine::Get().GetAssetManager().GetAssetHandle(texturePath);
                        submeshMaterial->UseNormalMap = true;
                    }
                    else
                    {
                        submeshMaterial->UseNormalMap = false;
                    }

                    submeshMaterial->EmissiveColor = glm::vec3(
                        (float)gltfMaterial.emissiveFactor[0],
                        (float)gltfMaterial.emissiveFactor[1],
                        (float)gltfMaterial.emissiveFactor[2]
                    );

                    if (gltfMaterial.emissiveTexture.index >= 0)
                    {
                        int imageIndex = model.textures[gltfMaterial.emissiveTexture.index].source;
                        std::string texturePath = std::filesystem::path(m_FilePath).parent_path().string() + "/" +
                            model.images[imageIndex].uri;
                        submeshMaterial->EmissiveTexture = Engine::Get().GetAssetManager().GetAssetHandle(texturePath);
                    }
                }
                else
                {
                    auto defaultTex = Engine::Get().GetAssetManager().GetDefaultTexture();
                    if (defaultTex)
                        submeshMaterial->AlbedoTexture = defaultTex->GetHandle();
                }

                auto [vb, ib] = Engine::Get().GetRenderer().CreateMeshBuffers(vertices, indices);
                Submesh sub;
                sub.VertexBuffer = vb;
                sub.IndexBuffer = ib;
                sub.IndexCount = (uint32_t)indices.size();
                sub.Material = submeshMaterial;
                sub.Name = mesh.name;

                m_Submeshes.push_back(sub);
            }
        }
    }
}
