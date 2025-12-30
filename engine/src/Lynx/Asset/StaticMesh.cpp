#include "StaticMesh.h"
#include "GLTFHelpers.h"
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>
#include <filesystem>

#include "Lynx/Engine.h"

namespace Lynx
{
    void TraverseNodes(const tinygltf::Model& model, const tinygltf::Node& node, const glm::mat4& parentTransform, std::vector<Submesh>& submeshes, const std::string& filePath, AABB& bounds);
    void ProcessMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const glm::mat4& transform, std::vector<Submesh>& submeshes, const std::string& filePath, AABB& bounds);
    
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
        m_Bounds = AABB();

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

        const tinygltf::Scene& scene = model.scenes[model.defaultScene > -1 ? model.defaultScene : 0];

        for (int nodeIndex : scene.nodes)
        {
            TraverseNodes(model, model.nodes[nodeIndex], glm::mat4(1.0f), m_Submeshes, m_FilePath, m_Bounds);
        }
    }

    void TraverseNodes(const tinygltf::Model& model, const tinygltf::Node& node, const glm::mat4& parentTransform, std::vector<Submesh>& submeshes, const std::string& filePath, AABB& bounds)
    {
        glm::mat4 localTransform = GLTFHelpers::GetLocalTransform(node);
        glm::mat4 globalTransform = parentTransform * localTransform;

        if (node.mesh >= 0)
        {
            ProcessMesh(model, model.meshes[node.mesh], globalTransform, submeshes, filePath, bounds);
        }

        for (int childIndex : node.children)
        {
            TraverseNodes(model, model.nodes[childIndex], globalTransform, submeshes, filePath, bounds);
        }
    }

    void ProcessMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const glm::mat4& transform, std::vector<Submesh>& submeshes, const std::string& filePath, AABB& bounds)
    {
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));

        for (const auto& primitive : mesh.primitives)
        {
            std::vector<float> positions;
            std::vector<float> normals;
            std::vector<float> tangents;
            std::vector<float> uvs;
            std::vector<float> colors;
            int colorComponents = 0;

            if (primitive.attributes.count("POSITION"))
                positions = GLTFHelpers::ReadFloatBuffer(model, model.accessors[primitive.attributes.at("POSITION")]);

            if (primitive.attributes.count("NORMAL"))
                normals = GLTFHelpers::ReadFloatBuffer(model, model.accessors[primitive.attributes.at("NORMAL")]);

            if (primitive.attributes.count("TANGENT"))
                tangents = GLTFHelpers::ReadFloatBuffer(model, model.accessors[primitive.attributes.at("TANGENT")]);

            if (primitive.attributes.count("TEXCOORD_0"))
                uvs = GLTFHelpers::ReadFloatBuffer(model, model.accessors[primitive.attributes.at("TEXCOORD_0")]);

            if (primitive.attributes.count("COLOR_0"))
            {
                const auto& accessor = model.accessors[primitive.attributes.at("COLOR_0")];
                colors = GLTFHelpers::ReadFloatBuffer(model, accessor);
                colorComponents = (accessor.type == TINYGLTF_TYPE_VEC3) ? 3 : 4;
            }

            size_t vertexCount = positions.size() / 3;
            std::vector<Vertex> vertices(vertexCount);

            for (size_t i = 0; i < vertexCount; ++i)
            {
                glm::vec3 pos(positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]);
                vertices[i].Position = glm::vec3(transform * glm::vec4(pos, 1.0f));
                bounds.Expand(vertices[i].Position);
                
                if (!normals.empty())
                {
                    glm::vec3 norm(normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]);
                    vertices[i].Normal = glm::normalize(normalMatrix * norm);
                }
                else
                {
                    vertices[i].Normal = {0, 1, 0};
                }

                if (!tangents.empty())
                {
                    glm::vec3 tan(tangents[i * 4], tangents[i * 4 + 1], tangents[i * 4 + 2]);
                    float w = tangents[i * 4 + 3];
                    vertices[i].Tangent = glm::vec4(glm::normalize(normalMatrix * tan), w);
                }
                else
                {
                    vertices[i].Tangent = {0, 0, 0, 1};
                }

                if (!uvs.empty())
                {
                    vertices[i].TexCoord = {uvs[i * 2], uvs[i * 2 + 1]};
                }
                else
                {
                    vertices[i].TexCoord = {0, 0};
                }

                if (!colors.empty())
                {
                    if (colorComponents == 3)
                        vertices[i].Color = { colors[i * 3], colors[i * 3 + 1], colors[i * 3 + 2], 1.0f };
                    else
                        vertices[i].Color = { colors[i * 4], colors[i * 4 + 1], colors[i * 4 + 2], colors[i * 4 + 3] };
                }
                else
                {
                    vertices[i].Color = {1, 1, 1, 1};
                }
            }

            std::vector<uint32_t> indices;
            if (primitive.indices >= 0)
            {
                const tinygltf::Accessor& accessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& indexView = model.bufferViews[accessor.bufferView];
                const unsigned char* indexData = &model.buffers[indexView.buffer].data[indexView.byteOffset + accessor.byteOffset];
                size_t indexStride = accessor.ByteStride(indexView);

                indices.reserve(accessor.count);
                for (size_t i = 0; i < accessor.count; ++i)
                {
                    uint32_t index = 0;
                    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                        index = *(const uint16_t*)(indexData + i * indexStride);
                    else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                        index = *(const uint32_t*)(indexData + i * indexStride);
                    else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                        index = *(const uint8_t*)(indexData + i * indexStride);
                    indices.push_back(index);
                }
            }
            else
            {
                indices.resize(vertexCount);
                for (size_t i = 0; i < vertexCount; ++i)
                    indices[i] = i;
            }

            if (normals.empty())
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

            if (tangents.empty())
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
                    std::string texturePath = std::filesystem::path(filePath).parent_path().string() + "/" +
                        model.images[imageIndex].uri;
                    submeshMaterial->AlbedoTexture = Engine::Get().GetAssetManager().GetAssetHandle(texturePath);
                }

                submeshMaterial->Metallic = (float)pbr.metallicFactor;
                submeshMaterial->Roughness = (float)pbr.roughnessFactor;

                if (pbr.metallicRoughnessTexture.index >= 0)
                {
                    int imageIndex = model.textures[pbr.metallicRoughnessTexture.index].source;
                    std::string texturePath = std::filesystem::path(filePath).parent_path().string() + "/" +
                        model.images[imageIndex].uri;
                    submeshMaterial->MetallicRoughnessTexture = Engine::Get().GetAssetManager().GetAssetHandle(texturePath);
                }

                if (gltfMaterial.normalTexture.index >= 0)
                {
                    int imageIndex = model.textures[gltfMaterial.normalTexture.index].source;
                    std::string texturePath = std::filesystem::path(filePath).parent_path().string() + "/" +
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
                    std::string texturePath = std::filesystem::path(filePath).parent_path().string() + "/" +
                        model.images[imageIndex].uri;
                    submeshMaterial->EmissiveTexture = Engine::Get().GetAssetManager().GetAssetHandle(texturePath);
                }

                if (gltfMaterial.alphaMode == "MASK")
                {
                    submeshMaterial->Mode = AlphaMode::Mask;
                    submeshMaterial->AlphaCutoff = (float)gltfMaterial.alphaCutoff;
                }
                else if (gltfMaterial.alphaMode == "BLEND")
                {
                    submeshMaterial->Mode = AlphaMode::Translucent;
                }
                else
                {
                    submeshMaterial->Mode = AlphaMode::Opaque;
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

            submeshes.push_back(sub);
        }
    }
}
