//
// Created by johnny on 2/25/26.
//

#pragma once
#include "common/VulkanInclude.hpp"
#include "TextureManager.hpp"
#include "common/Material.hpp"

#include "common/Vertex.hpp"

#include <string>
#include <vector>
#include <memory>


class Renderer;
struct Vertex;

/**
 * The TextureManager should own the Texture resources,
 * while the Model should own the Descriptor Sets.
 */
class Model {
public:
    // Constructor takes the allocator so it can create/destroy its own buffers
    // VmaAllocator is just a *pointer* to a struct.
    // Copying a VmaAllocator is just copying 8 bytes (the memory address).
    explicit Model(const VmaAllocator allocator, const vk::Device device)
        : allocator_(allocator), device_(device) {
    }

    ~Model();

    // Public API for the Renderer
    bool loadFromFile(const std::string &filePath, TextureManager &textureManager, Renderer &renderer);
    void uploadToGPU(vk::Device device, vk::Queue transferQueue, vk::CommandPool commandPool);

    // Getters so the Renderer can bind these to the Command Buffer
    [[nodiscard]] vk::Buffer getVertexBuffer() const { return vertexBuffer_.buffer; }
    [[nodiscard]] vk::Buffer getIndexBuffer() const { return indexBuffer_.buffer; }
    [[nodiscard]] uint32_t getIndexCount() const { return indexCount_; }

    [[nodiscard]] const std::vector<Submesh> &getSubmeshes() const { return submeshes_; }
    [[nodiscard]] const std::vector<Material> &getMaterials() const { return materials_; }

private:
    // Internal loaders
    // GPU Data: Managed by VMA
    struct AllocatedBuffer {
        vk::Buffer buffer = nullptr;
        VmaAllocation allocation = nullptr;
    } vertexBuffer_, indexBuffer_;


    bool loadGltf(const std::string &filePath, bool isBinary, TextureManager &textureManager, Renderer &renderer);

    VmaAllocator allocator_;
    vk::Device device_;

    // Sponza Data
    std::vector<Submesh> submeshes_;
    std::vector<Material> materials_;

    // --- THE CRITICAL ADDITION ---
    // This holds the actual GPU handles for images and views.
    // Without this, your Material DescriptorSets will point to deleted textures.
    std::vector<Texture> textures_;

    // CPU Data: These replace the globals!
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;

    uint32_t indexCount_ = 0;

};