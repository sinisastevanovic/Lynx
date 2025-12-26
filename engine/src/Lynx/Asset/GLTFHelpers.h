#pragma once

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#include <tiny_gltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Lynx::GLTFHelpers
{
    // Reads a generic float buffer from an accessor
    // Handles component types (BYTE, SHORT, FLOAT...) and normalization
    std::vector<float> ReadFloatBuffer(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
    {
        std::vector<float> output;

        size_t count = accessor.count;
        int numComponents = 1;
        if (accessor.type == TINYGLTF_TYPE_SCALAR)
            numComponents = 1;
        else if (accessor.type == TINYGLTF_TYPE_VEC2)
            numComponents = 2;
        else if (accessor.type == TINYGLTF_TYPE_VEC3)
            numComponents = 3;
        else if (accessor.type == TINYGLTF_TYPE_VEC4)
            numComponents = 4;

        output.resize(count * numComponents);

        const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[view.buffer];
        const unsigned char* dataStart = &buffer.data[view.byteOffset + accessor.byteOffset];
        int stride = accessor.ByteStride(view);

        for (size_t i = 0; i < count; ++i)
        {
            const unsigned char* ptr = dataStart + i * stride;

            for (int c = 0; c < numComponents; ++c)
            {
                float value = 0.0f;

                if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
                {
                    value = ((const float*)ptr)[c];
                }
                else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_BYTE)
                {
                    char v = ((const char*)ptr)[c];
                    value = accessor.normalized ? (float)v / 127.0f : (float)v;
                }
                else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                {
                    unsigned char v = ((const unsigned char*)ptr)[c];
                    value = accessor.normalized ? (float)v / 255.0f : (float)v;
                }
                else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_SHORT)
                {
                    short v = ((const short*)ptr)[c];
                    value = accessor.normalized ? (float)v / 32767.0f : (float)v;
                }
                else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                {
                    unsigned short v = ((const unsigned short*)ptr)[c];
                    value = accessor.normalized ? (float)v / 65535.0f : (float)v;
                }
                // TODO: INT/UINT

                output[i * numComponents + c] = value;
            }
        }

        if (accessor.sparse.isSparse)
        {
            LX_CORE_WARN("GLTF Sparse Accessor not fully supported!");
        }

        return output;
    }

    glm::mat4 GetLocalTransform(const tinygltf::Node& node)
    {
        glm::mat4 transform(1.0f);

        if (node.matrix.size() == 16)
        {
            transform = glm::make_mat4(node.matrix.data());
        }
        else
        {
            if (node.translation.size() == 3)
            {
                transform = glm::translate(transform, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
            }
            if (node.rotation.size() == 4)
            {
                glm::quat q(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
                transform = transform * glm::mat4(q);
            }
            if (node.scale.size() == 3)
            {
                transform = glm::scale(transform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
            }
        }
        return transform;
    }
}