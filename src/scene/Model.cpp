//
// Created by johnny on 2/25/26.
//

#include "Model.hpp"
#include "system/GltfLoader.hpp"
// #include "external/vendor_impl.cpp" // This includes the tinyobj implementation
#include <tiny_obj_loader.h>
#include <stb_image.h>
#include <tiny_gltf.h>
#include <unordered_map>
#include "../vulkan/VulkanUtils.hpp"
#include <iostream>

Model::~Model() {
    // VMA is safe: it ignores null handles, but checking is cleaner
    if (vertexBuffer_.buffer) {
        vmaDestroyBuffer(allocator_, static_cast<VkBuffer>(vertexBuffer_.buffer), vertexBuffer_.allocation);
        vertexBuffer_.buffer = nullptr; // Optional but clean
        vertexBuffer_.allocation = nullptr;
    }

    if (indexBuffer_.buffer) {
        vmaDestroyBuffer(allocator_, static_cast<VkBuffer>(indexBuffer_.buffer), indexBuffer_.allocation);
        indexBuffer_.buffer = nullptr;
        indexBuffer_.allocation = nullptr;
    }
}

void Model::loadFromFile(const std::string &filePath) {
    std::string ext = filePath.substr(filePath.find_last_of('.') + 1);

    if (ext == "obj") {
        loadObj(filePath);
    } else if (ext == "gltf" || ext == "glb") {
        loadGltf(filePath);
    } else {
        throw std::runtime_error("Unsupported model format: " + ext);
    }
}

void Model::loadGltf(const std::string &filePath) {
    auto data = GltfLoader::loadFromFile(filePath);
    // Move the data to avoid expensive copying of large vectors
    vertices_ = std::move(data.vertices);
    indices_ = std::move(data.indices);
    indexCount_ = static_cast<uint32_t>(indices_.size());
}

void Model::loadObj(const std::string &filePath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath.c_str())) {
        throw std::runtime_error("TinyOBJ Loader Error: " + warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto &shape : shapes) {
        for (const auto &index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (index.normal_index >= 0) {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }

            if (index.texcoord_index >= 0) {
                vertex.uv = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }

            vertex.color = {1.0f, 1.0f, 1.0f}; // Default color

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices_.size());
                vertices_.push_back(vertex);
            }
            indices_.push_back(uniqueVertices[vertex]);
        }
    }
    indexCount_ = static_cast<uint32_t>(indices_.size());
}

void Model::uploadToGPU(vk::Device device, vk::Queue queue, vk::CommandPool commandPool) {
    // Vertex Buffer
    createAndUploadBuffer(device,
                          queue,
                          commandPool,
                          vertices_.size() * sizeof(Vertex),
                          vk::BufferUsageFlagBits::eVertexBuffer,
                          vertices_.data(),
                          vertexBuffer_
        );

    // Index Buffer
    createAndUploadBuffer(device,
                          queue, commandPool,
                          indices_.size() * sizeof(uint32_t),
                          vk::BufferUsageFlagBits::eIndexBuffer,
                          indices_.data(),
                          indexBuffer_
        );

    // --- NEW: Discard CPU data ---
    // We clear the vectors because this is a rigid body.
    // We don't need to perform CPU-side skinning or deformation.
    vertices_.clear();
    vertices_.shrink_to_fit();

    indices_.clear();
    indices_.shrink_to_fit();

    std::cout << "[Model] GPU upload complete. CPU memory cleared." << std::endl;
}

void Model::createAndUploadBuffer(
    vk::Device device,
    vk::Queue queue,
    vk::CommandPool commandPool,
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    const void *data,
    AllocatedBuffer &outBuffer) const {
    // 1. Create Staging Buffer (CPU Visible)
    vk::Buffer stagingBuffer;
    VmaAllocation stagingAlloc;
    VulkanUtils::createBuffer(
        allocator_, size,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_CPU_ONLY,
        stagingBuffer, stagingAlloc
        );

    // 2. Copy CPU data to Staging
    void *mappedData;
    vmaMapMemory(allocator_, stagingAlloc, &mappedData);
    memcpy(mappedData, data, static_cast<size_t>(size));
    vmaUnmapMemory(allocator_, stagingAlloc);

    // 3. Create GPU Buffer (Device Local)
    VulkanUtils::createBuffer(
        allocator_, size,
        vk::BufferUsageFlagBits::eTransferDst | usage,
        VMA_MEMORY_USAGE_GPU_ONLY,
        outBuffer.buffer, outBuffer.allocation
        );

    // 4. Copy from Staging to GPU
    // Note: This uses your VulkanUtils helper which usually handles
    // the fence/submission internally.
    VulkanUtils::copyBuffer(device, commandPool, queue, stagingBuffer, outBuffer.buffer, size);

    // 5. Cleanup Staging
    vmaDestroyBuffer(allocator_, stagingBuffer, stagingAlloc);
}