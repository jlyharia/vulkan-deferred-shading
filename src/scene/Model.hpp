//
// Created by johnny on 2/25/26.
//

#pragma once
#include "common/VulkanInclude.hpp"
#include "common/Material.hpp"

#include <vector>
#include <memory>

/**
 * A "dumb" GPU data container. Owns vertex/index buffers and a list of Materials.
 * Does not know how to load files or upload data — AssetManager handles that.
 * Texture lifetimes are managed by shared_ptr inside each Material.
 */
class Model {
public:
    explicit Model(VmaAllocator allocator, vk::Device device);
    ~Model();

    Model(const Model &) = delete;
    Model &operator=(const Model &) = delete;

    // --- Called by AssetManager ---

    /** @brief Receives the GPU buffer handles after AssetManager uploads geometry. */
    void setGpuResources(vk::Buffer vertexBuf, VmaAllocation vertexAlloc,
                         vk::Buffer indexBuf,  VmaAllocation indexAlloc,
                         uint32_t indexCount);

    /** @brief Appends a fully-assembled Material (with descriptor set) to this model. */
    void addMaterial(const Material &material);

    /** @brief Replaces the submesh list (called once after geometry upload). */
    void setSubmeshes(std::vector<Submesh> submeshes);

    // --- Renderer API ---

    [[nodiscard]] vk::Buffer getVertexBuffer() const { return vertexBuffer_.buffer; }
    [[nodiscard]] vk::Buffer getIndexBuffer()  const { return indexBuffer_.buffer; }
    [[nodiscard]] uint32_t   getIndexCount()   const { return indexCount_; }
    [[nodiscard]] const std::vector<Submesh>  &getSubmeshes()  const { return submeshes_; }
    [[nodiscard]] const std::vector<Material> &getMaterials()  const { return materials_; }

private:
    struct AllocatedBuffer {
        vk::Buffer    buffer     = nullptr;
        VmaAllocation allocation = nullptr;
    } vertexBuffer_, indexBuffer_;

    VmaAllocator allocator_;
    vk::Device   device_;
    uint32_t     indexCount_ = 0;

    std::vector<Submesh>  submeshes_;
    std::vector<Material> materials_;
};
