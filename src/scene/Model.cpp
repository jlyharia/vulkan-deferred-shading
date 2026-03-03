//
// Created by johnny on 2/25/26.
//

#include "Model.hpp"
#include "system/GltfLoader.hpp"
#include <tiny_obj_loader.h>
#include <tiny_gltf.h>
#include <unordered_map>
#include "../vulkan/VulkanUtils.hpp"

#include <fstream>
#include <iostream>
#include <algorithm> // Required for std::ranges::transform
#include <cctype>    // Required for ::tolower

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

bool Model::loadFromFile(const std::string &filePath) {
    namespace fs = std::filesystem;
    fs::path path(filePath);

    // 1. Check if file exists
    if (!fs::exists(path)) {
        std::cerr << "[Model] File does not exist: " << filePath << std::endl;
        return false;
    }

    // 2. Extract extension and normalize to lowercase
    std::string ext = path.extension().string();
    // std::ranges::transform(ext, ext.begin(), ::tolower);
    // 2. Modern C++20 Lowercase Transformation
    std::ranges::transform(ext, ext.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    // 3. Handle OBJ directly
    if (ext == ".obj") {
        return loadObj(filePath);
    }

    // 4. Handle glTF / glb using Header Detection (Magic Number)
    if (ext == ".gltf" || ext == ".glb") {
        std::ifstream file(filePath, std::ios::binary);
        if (!file)
            return false;

        char header[4];
        file.read(header, 4);
        file.close();

        // "glTF" in ASCII is the magic number for binary .glb
        bool isBinary = (std::strncmp(header, "glTF", 4) == 0);

        return loadGltf(filePath, isBinary);
    }

    std::cerr << "[Model] Unsupported file extension: " << ext << std::endl;
    return false;
}

bool Model::loadGltf(const std::string &filePath, bool isBinary) {
    // We pass the isBinary flag we detected in the header check
    auto data = GltfLoader::loadFromFile(filePath, isBinary);

    if (!data.success) {
        return false;
    }

    // Clear old data for re-loading safety
    vertices_.clear();
    indices_.clear();
    submeshes_.clear();

    // Move data (Zero-copy)
    vertices_ = std::move(data.vertices);
    indices_ = std::move(data.indices);
    indexCount_ = static_cast<uint32_t>(indices_.size());

    for (const auto &m : data.meshes) {
        submeshes_.push_back({m.firstIndex, m.indexCount, m.materialIndex});
    }

    std::cout << "[Model] Loaded glTF: " << filePath << " (" << vertices_.size() << " verts)" << std::endl;
    return true;
}

bool Model::loadObj(const std::string &filePath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    // 1. Attempt to load the file
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath.c_str())) {
        std::cerr << "[TinyOBJ] Error: " << err << " (Warning: " << warn << ")" << std::endl;
        return false; // Graceful failure
    }

    // Optional: Log warnings even if loading succeeded
    if (!warn.empty()) {
        std::cout << "[TinyOBJ] Warning: " << warn << std::endl;
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    // Clear existing data in case this is a re-load
    vertices_.clear();
    indices_.clear();

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

            vertex.color = {1.0f, 1.0f, 1.0f};

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices_.size());
                vertices_.push_back(vertex);
            }
            indices_.push_back(uniqueVertices[vertex]);
        }
    }

    indexCount_ = static_cast<uint32_t>(indices_.size());
    submeshes_.push_back({0, static_cast<uint32_t>(indices_.size()), -1});

    // 2. Final check: did we actually get any geometry?
    if (vertices_.empty()) {
        std::cerr << "[Model] Error: No geometry found in " << filePath << std::endl;
        return false;
    }

    return true; // Success!
}

void Model::uploadToGPU(vk::Device device, vk::Queue queue, vk::CommandPool commandPool) {

    if (vertices_.empty() || indices_.empty()) {
        std::cerr << "[Model Error] Attempted to upload empty mesh data to GPU! Check loader." << std::endl;
        return;
    }
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