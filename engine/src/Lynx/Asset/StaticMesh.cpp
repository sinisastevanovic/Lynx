#include "StaticMesh.h"
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>
#include <filesystem>

#include "Lynx/Engine.h"

namespace Lynx
{
    StaticMesh::StaticMesh(nvrhi::DeviceHandle device, const std::string& filepath)
    {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
        if (!ret)
        {
            LX_CORE_ERROR("Failed to load glTF: {0}", err);
            return;
        }

        // Extract data
        const tinygltf::Mesh& mesh = model.meshes[0];
        const tinygltf::Primitive& primitive = mesh.primitives[0];

        // Get positions
        const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
        const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
        const unsigned char* posData = &model.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset];
        int posStride = posAccessor.ByteStride(posView);

        // Get TexCoords
        const tinygltf::Accessor& uvAccessor   = model.accessors[primitive.attributes.at("TEXCOORD_0")];
        const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
        const unsigned char* uvData = &model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset];
        int uvStride = uvAccessor.ByteStride(uvView);
        
        std::vector<Vertex> vertices(posAccessor.count);
        for (size_t i = 0; i < posAccessor.count; ++i)
        {
            const float* p = reinterpret_cast<const float*>(posData + i * posStride);
            const float* t = reinterpret_cast<const float*>(uvData + i * uvStride);
            vertices[i].Position = { p[0], p[1], p[2] };
            vertices[i].TexCoord = { t[0], t[1] };
        }

        // Get indices
        const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
        const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
        const unsigned char* indexPtr = &model.buffers[indexView.buffer].data[indexAccessor.byteOffset + indexView.byteOffset];

        std::vector<uint32_t> indices;
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
        
        m_IndexCount = (uint32_t)indices.size();
        
        auto vbDesc = nvrhi::BufferDesc()
            .setByteSize(vertices.size() * sizeof(Vertex))
            .setIsVertexBuffer(true)
            .setDebugName("MeshVertexBuffer")
            .enableAutomaticStateTracking(nvrhi::ResourceStates::CopyDest);
        m_VertexBuffer = device->createBuffer(vbDesc);

        auto ibDesc = nvrhi::BufferDesc()
            .setByteSize(indices.size() * sizeof(uint32_t))
            .setIsIndexBuffer(true)
            .setDebugName("MeshIndexBuffer")
            .enableAutomaticStateTracking(nvrhi::ResourceStates::CopyDest);
        m_IndexBuffer = device->createBuffer(ibDesc);

        if (!model.textures.empty())
        {
            int imageIndex = model.textures[0].source;
            std::string texturePath = std::filesystem::path(filepath).parent_path().string() + "/" + model.images[imageIndex].uri;
            auto tex = Engine::Get().GetAssetManager().GetTexture(texturePath);
            SetTexture(tex->GetHandle());
        }

        // TODO: We use a temporary command list here to immediately upload the buffers,
        // in the future, we should maybe batch these things? 
        nvrhi::CommandListHandle commandList = device->createCommandList();
        commandList->open();
        commandList->writeBuffer(m_VertexBuffer, vertices.data(), vbDesc.byteSize);
        commandList->writeBuffer(m_IndexBuffer, indices.data(), ibDesc.byteSize);
        commandList->close();
        device->executeCommandList(commandList);
    }
}

