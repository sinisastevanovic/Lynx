#include "StaticMesh.h"
#include "GLTFHelpers.h"
#include "MeshCache.h"
#include "MeshCooking.h"
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>
#include <filesystem>

#include "Lynx/Engine.h"

namespace Lynx
{
    void TraverseNodes(const tinygltf::Model& model, const tinygltf::Node& node, const glm::mat4& parentTransform, std::vector<SubmeshSourceData>& submeshes, const std::string& filePath, AABB& bounds);
    void ProcessMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const glm::mat4& transform, std::vector<SubmeshSourceData>& submeshes, const std::string& filePath, AABB& bounds);
    
    StaticMesh::StaticMesh(const std::string& filepath)
        : Asset(filepath)
    {
    }

    StaticMesh::StaticMesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices)
    {
        auto submeshMaterial = std::make_shared<Material>();
        auto defaultTex = Engine::Get().GetAssetManager().GetDefaultTexture();
        if (defaultTex)
            submeshMaterial->AlbedoTexture = defaultTex->GetHandle();

        Engine::Get().GetAssetManager().AddRuntimeAsset(submeshMaterial);

        // The box around the whole mesh. The renderer culls with a sphere per submesh instead.
        for (const Vertex& vertex : vertices)
            m_Bounds.Expand(vertex.Position);

        const std::vector<PackedVertex> packed = PackVertices(vertices);

        Submesh sub;
        sub.Geometry = Engine::Get().GetRenderer().CreateMeshGeometry(packed, indices);
        sub.BoundingSphere = ComputeBoundingSphere(packed);
        sub.IndexCount = (uint32_t)indices.size();
        sub.Lods = { MeshLod{ 0, sub.IndexCount, 0.0f } };
        sub.Material = submeshMaterial;
        sub.Name = "RuntimeCreated";

        m_Submeshes.push_back(sub);
        m_State = AssetState::Ready;
    }

    bool StaticMesh::Reload()
    {
        if (m_FilePath.empty())
            return false;

        if (LoadSourceData())
        {
            return CreateRenderResources();
        }
        return false;
    }

    bool StaticMesh::LoadSourceData()
    {
        m_Submeshes.clear();
        m_Bounds = AABB();

        // A mesh that was cooked before, from files that have not changed since, needs none of what follows.
        const MeshCache& cache = Engine::Get().GetMeshCache();
        if (std::optional<CookedMesh> cooked = cache.Load(m_FilePath))
        {
            m_Bounds = cooked->Bounds;
            m_SourceData = std::move(cooked->Submeshes);
            return !m_SourceData.empty();
        }

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
            return false;
        }

        const tinygltf::Scene& scene = model.scenes[model.defaultScene > -1 ? model.defaultScene : 0];

        for (int nodeIndex : scene.nodes)
        {
            TraverseNodes(model, model.nodes[nodeIndex], glm::mat4(1.0f), m_SourceData, m_FilePath, m_Bounds);
        }

        for (SubmeshSourceData& submesh : m_SourceData)
        {
            OptimizeDrawOrder(submesh.Vertices, submesh.Indices);
            // A surface with cut-outs that is made of many separate pieces is foliage, and gets its levels
            // by drawing fewer and larger leaves. Everything else gets them by making its surface simpler.
            const bool hasCutOuts = submesh.MaterialData.Mode == AlphaMode::Mask;
            Foliage foliage;
            if (hasCutOuts)
                foliage = BuildFoliageLods(submesh.Vertices, submesh.Indices);
            const bool isFoliage = !foliage.Lods.empty();
            submesh.Lods = isFoliage ? std::move(foliage.Lods) : BuildLods(submesh.Vertices, submesh.Indices, hasCutOuts);
            if (submesh.Lods.size() > 1)
            {
                std::string triangles;
                for (const MeshLod& lod : submesh.Lods)
                    triangles += (triangles.empty() ? "" : ", ") + std::to_string(lod.IndexCount / 3);
                if (isFoliage)
                    LX_CORE_INFO("Levels of detail of {0} in {1}, foliage with leaves of {3} units, in triangles: {2}.", submesh.Name, m_FilePath, triangles, foliage.LeafSize);
                else
                    LX_CORE_INFO("Levels of detail of {0} in {1}, in triangles: {2}. The coarsest is off by {3} units.", submesh.Name, m_FilePath, triangles, submesh.Lods.back().Error);
            }

            submesh.PackedVertices = PackVertices(submesh.Vertices);
            submesh.BoundingSphere = ComputeBoundingSphere(submesh.PackedVertices);
            if (isFoliage)
            {
                submesh.Leaves = PackLeaves(foliage.Leaves, submesh.BoundingSphere);
                submesh.LeafSize = foliage.LeafSize;
            }
            submesh.Vertices = {};
        }

        // The mesh was made from the model file and the files that hold its vertex data.
        // Its textures are assets of their own and not part of it.
        std::vector<std::filesystem::path> dependencies = { m_FilePath };
        const std::filesystem::path directory = std::filesystem::path(m_FilePath).parent_path();
        for (const tinygltf::Buffer& buffer : model.buffers)
        {
            const bool isFile = !buffer.uri.empty() && !buffer.uri.starts_with("data:");
            if (isFile)
                dependencies.push_back(directory / buffer.uri);
        }
        cache.Store(m_FilePath, dependencies, CookedMesh{ m_Bounds, m_SourceData });

        return !m_SourceData.empty();
    }

    bool StaticMesh::CreateRenderResources()
    {
        m_Submeshes.clear();

        // TODO: We should add a JobSystem to be able to load multiple assets simoultaniously here.
        // So we don't need this intermediate data. 

        for (const auto& source : m_SourceData)
        {
            auto material = std::make_shared<Material>();
            material->AlbedoColor = source.MaterialData.AlbedoColor;
            material->Metallic = source.MaterialData.Metallic;
            material->Roughness = source.MaterialData.Roughness;
            material->EmissiveColor = source.MaterialData.EmissiveColor;
            material->EmissiveStrength = source.MaterialData.EmissiveStrength;
            material->Mode = source.MaterialData.Mode;
            material->AlphaCutoff = source.MaterialData.AlphaCutoff;
            material->TwoSided = source.MaterialData.TwoSided;

            // A model file says what each of its pictures is, which is more than a picture says of
            // itself. It is passed on as a first setting for textures that have none yet.
            auto& am = Engine::Get().GetAssetManager();
            if (!source.MaterialData.AlbedoPath.empty())
            {
                material->AlbedoTexture = am.GetAssetHandle(source.MaterialData.AlbedoPath);
                if (material->Mode == AlphaMode::Mask)
                    am.SuggestCutOutTexture(material->AlbedoTexture);
                else
                    am.SuggestTextureContent(material->AlbedoTexture, TextureContent::Color);
            }
            else if (auto def = am.GetDefaultTexture())
            {
                material->AlbedoTexture = def->GetHandle();
            }

            if (!source.MaterialData.MetallicRoughnessPath.empty())
            {
                material->MetallicRoughnessTexture = am.GetAssetHandle(source.MaterialData.MetallicRoughnessPath);
                am.SuggestTextureContent(material->MetallicRoughnessTexture, TextureContent::Data);
            }
            if (!source.MaterialData.NormalPath.empty())
            {
                material->NormalMap = am.GetAssetHandle(source.MaterialData.NormalPath);
                material->UseNormalMap = true;
                am.SuggestTextureContent(material->NormalMap, TextureContent::NormalMap);
            }
            if (!source.MaterialData.EmissivePath.empty())
            {
                material->EmissiveTexture = am.GetAssetHandle(source.MaterialData.EmissivePath);
                am.SuggestTextureContent(material->EmissiveTexture, TextureContent::Color);
            }

            Engine::Get().GetAssetManager().AddRuntimeAsset(material);

            Submesh sub;
            sub.Geometry = Engine::Get().GetRenderer().CreateMeshGeometry(source.PackedVertices, source.Indices);
            sub.BoundingSphere = source.BoundingSphere;
            sub.Lods = source.Lods;
            if (!source.Leaves.empty())
            {
                sub.LeafData = Engine::Get().GetRenderer().CreateLeafData(source.Leaves);
                sub.LeafSize = source.LeafSize;
            }
            sub.IndexCount = source.Lods.front().IndexCount;
            sub.Material = material;
            sub.Name = source.Name;
            m_Submeshes.push_back(sub);
        }

        m_SourceData.clear();
        IncrementVersion();
        return true;
    }

    void TraverseNodes(const tinygltf::Model& model, const tinygltf::Node& node, const glm::mat4& parentTransform, std::vector<SubmeshSourceData>& submeshes, const std::string& filePath, AABB& bounds)
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

    void ProcessMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const glm::mat4& transform, std::vector<SubmeshSourceData>& submeshes, const std::string& filePath, AABB& bounds)
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

            SubmeshSourceData sourceData;

            size_t vertexCount = positions.size() / 3;
            sourceData.Vertices.resize(vertexCount);

            for (size_t i = 0; i < vertexCount; ++i)
            {
                glm::vec3 pos(positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]);
                sourceData.Vertices[i].Position = glm::vec3(transform * glm::vec4(pos, 1.0f));
                bounds.Expand(sourceData.Vertices[i].Position);
                
                if (!normals.empty())
                {
                    glm::vec3 norm(normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]);
                    sourceData.Vertices[i].Normal = glm::normalize(normalMatrix * norm);
                }
                else
                {
                    sourceData.Vertices[i].Normal = {0, 1, 0};
                }

                if (!tangents.empty())
                {
                    glm::vec3 tan(tangents[i * 4], tangents[i * 4 + 1], tangents[i * 4 + 2]);
                    float w = tangents[i * 4 + 3];
                    sourceData.Vertices[i].Tangent = glm::vec4(glm::normalize(normalMatrix * tan), w);
                }
                else
                {
                    sourceData.Vertices[i].Tangent = {0, 0, 0, 1};
                }

                if (!uvs.empty())
                {
                    sourceData.Vertices[i].TexCoord = {uvs[i * 2], uvs[i * 2 + 1]};
                }
                else
                {
                    sourceData.Vertices[i].TexCoord = {0, 0};
                }

                if (!colors.empty())
                {
                    if (colorComponents == 3)
                        sourceData.Vertices[i].Color = { colors[i * 3], colors[i * 3 + 1], colors[i * 3 + 2], 1.0f };
                    else
                        sourceData.Vertices[i].Color = { colors[i * 4], colors[i * 4 + 1], colors[i * 4 + 2], colors[i * 4 + 3] };
                }
                else
                {
                    sourceData.Vertices[i].Color = {1, 1, 1, 1};
                }
            }

            if (primitive.indices >= 0)
            {
                const tinygltf::Accessor& accessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& indexView = model.bufferViews[accessor.bufferView];
                const unsigned char* indexData = &model.buffers[indexView.buffer].data[indexView.byteOffset + accessor.byteOffset];
                size_t indexStride = accessor.ByteStride(indexView);

                sourceData.Indices.reserve(accessor.count);
                for (size_t i = 0; i < accessor.count; ++i)
                {
                    uint32_t index = 0;
                    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                        index = *(const uint16_t*)(indexData + i * indexStride);
                    else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                        index = *(const uint32_t*)(indexData + i * indexStride);
                    else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                        index = *(const uint8_t*)(indexData + i * indexStride);
                    sourceData.Indices.push_back(index);
                }
            }
            else
            {
                sourceData.Indices.resize(vertexCount);
                for (size_t i = 0; i < vertexCount; ++i)
                    sourceData.Indices[i] = i;
            }

            if (normals.empty())
            {
                for (size_t i = 0; i < sourceData.Indices.size(); i += 3)
                {
                    uint32_t idx0 = sourceData.Indices[i+0];
                    uint32_t idx1 = sourceData.Indices[i+1];
                    uint32_t idx2 = sourceData.Indices[i+2];

                    glm::vec3 v0 = sourceData.Vertices[idx0].Position;
                    glm::vec3 v1 = sourceData.Vertices[idx1].Position;
                    glm::vec3 v2 = sourceData.Vertices[idx2].Position;

                    glm::vec3 edge1 = v1 - v0;
                    glm::vec3 edge2 = v2 - v0;
                    glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

                    sourceData.Vertices[idx0].Normal += normal;
                    sourceData.Vertices[idx1].Normal += normal;
                    sourceData.Vertices[idx2].Normal += normal;
                }

                for (auto& v : sourceData.Vertices)
                {
                    if (glm::length(v.Normal) > 0.0f)
                        v.Normal = glm::normalize(v.Normal);
                }
            }

            if (tangents.empty())
            {
                // For every vertex, the directions in which the texture's two axes run across the surface,
                // summed over its triangles. A larger triangle counts for more.
                std::vector<glm::vec3> alongU(vertexCount, glm::vec3(0.0f));
                std::vector<glm::vec3> alongV(vertexCount, glm::vec3(0.0f));

                for (size_t i = 0; i + 2 < sourceData.Indices.size(); i += 3)
                {
                    const uint32_t corners[3] = { sourceData.Indices[i], sourceData.Indices[i + 1], sourceData.Indices[i + 2] };
                    const Vertex& v0 = sourceData.Vertices[corners[0]];
                    const Vertex& v1 = sourceData.Vertices[corners[1]];
                    const Vertex& v2 = sourceData.Vertices[corners[2]];

                    const glm::vec3 edge1 = v1.Position - v0.Position;
                    const glm::vec3 edge2 = v2.Position - v0.Position;
                    const glm::vec2 deltaUV1 = v1.TexCoord - v0.TexCoord;
                    const glm::vec2 deltaUV2 = v2.TexCoord - v0.TexCoord;

                    // Only the sign of the area in the texture is used, and the two directions are normalised.
                    // Dividing by the area instead goes wrong on a detailed mesh, whose triangles are tiny in
                    // the texture: a threshold on it would drop nearly all of them.
                    const float areaInTexture = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
                    if (areaInTexture == 0.0f)
                        continue;

                    const float direction = areaInTexture < 0.0f ? -1.0f : 1.0f;
                    const glm::vec3 u = direction * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
                    const glm::vec3 v = direction * (deltaUV1.x * edge2 - deltaUV2.x * edge1);
                    const float lengthU = glm::length(u);
                    const float lengthV = glm::length(v);
                    if (!(lengthU > 0.0f) || !(lengthV > 0.0f) || !std::isfinite(lengthU) || !std::isfinite(lengthV))
                        continue;

                    const float weight = glm::length(glm::cross(edge1, edge2));
                    for (const uint32_t corner : corners)
                    {
                        alongU[corner] += u * (weight / lengthU);
                        alongV[corner] += v * (weight / lengthV);
                    }
                }

                for (size_t i = 0; i < vertexCount; ++i)
                {
                    Vertex& vertex = sourceData.Vertices[i];
                    const glm::vec3 normal = vertex.Normal;

                    // The tangent lies in the surface, at a right angle to the normal.
                    glm::vec3 tangent = alongU[i] - normal * glm::dot(normal, alongU[i]);
                    if (!(glm::dot(tangent, tangent) > 1e-12f))
                    {
                        // The texture gives no direction here. Any direction in the surface will do, as long
                        // as it is one: a tangent along the normal makes the shader divide by zero.
                        const glm::vec3 other = std::abs(normal.x) < 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
                        tangent = other - normal * glm::dot(normal, other);
                    }
                    tangent = glm::normalize(tangent);

                    // The third direction is cross(normal, tangent) times w, and points up in the texture,
                    // which is against v. Where the texture is mirrored on the surface, w is -1.
                    const float w = glm::dot(glm::cross(normal, tangent), -alongV[i]) < 0.0f ? -1.0f : 1.0f;
                    vertex.Tangent = glm::vec4(tangent, w);
                }
            }

            if (primitive.material >= 0)
            {
                const tinygltf::Material& gltfMaterial = model.materials[primitive.material];
                const auto& pbr = gltfMaterial.pbrMetallicRoughness;
                
                sourceData.MaterialData.AlbedoColor = glm::vec4(
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
                    sourceData.MaterialData.AlbedoPath = texturePath;
                }

                sourceData.MaterialData.Metallic = (float)pbr.metallicFactor;
                sourceData.MaterialData.Roughness = (float)pbr.roughnessFactor;

                if (pbr.metallicRoughnessTexture.index >= 0)
                {
                    int imageIndex = model.textures[pbr.metallicRoughnessTexture.index].source;
                    std::string texturePath = std::filesystem::path(filePath).parent_path().string() + "/" +
                        model.images[imageIndex].uri;
                    sourceData.MaterialData.MetallicRoughnessPath = texturePath;
                }

                if (gltfMaterial.normalTexture.index >= 0)
                {
                    int imageIndex = model.textures[gltfMaterial.normalTexture.index].source;
                    std::string texturePath = std::filesystem::path(filePath).parent_path().string() + "/" +
                        model.images[imageIndex].uri;
                    sourceData.MaterialData.NormalPath = texturePath;
                }

                sourceData.MaterialData.EmissiveColor = glm::vec3(
                    (float)gltfMaterial.emissiveFactor[0],
                    (float)gltfMaterial.emissiveFactor[1],
                    (float)gltfMaterial.emissiveFactor[2]
                );

                if (gltfMaterial.extensions.count("KHR_materials_emissive_strength"))
                {
                    const auto& ext = gltfMaterial.extensions.at("KHR_materials_emissive_strength");
                    if (ext.Has("emissiveStrength"))
                    {
                        sourceData.MaterialData.EmissiveStrength = (float)ext.Get("emissiveStrength").GetNumberAsDouble();
                    }
                }
                
                if (gltfMaterial.emissiveTexture.index >= 0)
                {
                    int imageIndex = model.textures[gltfMaterial.emissiveTexture.index].source;
                    std::string texturePath = std::filesystem::path(filePath).parent_path().string() + "/" +
                        model.images[imageIndex].uri;
                    sourceData.MaterialData.EmissivePath = texturePath;
                }

                if (gltfMaterial.alphaMode == "MASK")
                {
                    sourceData.MaterialData.Mode = AlphaMode::Mask;
                    sourceData.MaterialData.AlphaCutoff = (float)gltfMaterial.alphaCutoff;
                }
                else if (gltfMaterial.alphaMode == "BLEND")
                {
                    sourceData.MaterialData.Mode = AlphaMode::Translucent;
                }
                else
                {
                    sourceData.MaterialData.Mode = AlphaMode::Opaque;
                }

                sourceData.MaterialData.TwoSided = gltfMaterial.doubleSided;
            }
            sourceData.Name = mesh.name;
            submeshes.push_back(sourceData);
        }
    }
}
