//
// Created by johnny on 2/25/26.
//

#pragma once

#include "common/VulkanInclude.hpp"
#include "common/Vertex.hpp"
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

struct Vertex;

class Model {
public:
    // Constructor takes the allocator so it can create/destroy its own buffers
    // VmaAllocator is just a *pointer* to a struct.
    // Copying a VmaAllocator is just copying 8 bytes (the memory address).
    Model(VmaAllocator allocator) : allocator_(allocator) {
    }

    ~Model();

    // Public API for the Renderer
    bool loadFromFile(const std::string &filePath);
    void uploadToGPU(vk::Device device, vk::Queue transferQueue, vk::CommandPool commandPool);

    // Getters so the Renderer can bind these to the Command Buffer
    [[nodiscard]] vk::Buffer getVertexBuffer() const { return vertexBuffer_.buffer; }
    [[nodiscard]] vk::Buffer getIndexBuffer() const { return indexBuffer_.buffer; }
    // [[nodiscard]] uint32_t getIndexCount() const { return static_cast<uint32_t>(indices_.size()); }
    [[nodiscard]] uint32_t getIndexCount() const { return indexCount_; }

    struct Submesh {
        uint32_t firstIndex;
        uint32_t indexCount;
        int materialIndex;
    };

    [[nodiscard]] const std::vector<Submesh> &getSubmeshes() const { return submeshes_; }

private:
    // Internal loaders
    // GPU Data: Managed by VMA
    struct AllocatedBuffer {
        vk::Buffer buffer = nullptr;
        VmaAllocation allocation = nullptr;
    } vertexBuffer_, indexBuffer_;


    bool loadObj(const std::string &filePath);
    // bool loadGltf(const std::string &filePath);
    bool loadGltf(const std::string &filePath, bool isBinary);

    void createAndUploadBuffer(
        vk::Device device,
        vk::Queue queue,
        vk::CommandPool commandPool,
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        const void *data,
        AllocatedBuffer &outBuffer) const;
    VmaAllocator allocator_;

    // CPU Data: These replace the globals!
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
    std::vector<Submesh> submeshes_;
    uint32_t indexCount_ = 0;

};