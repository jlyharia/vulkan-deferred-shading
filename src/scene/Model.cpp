//
// Created by johnny on 2/25/26.
//

#include "Model.hpp"

Model::Model(const VmaAllocator allocator, const vk::Device device)
    : allocator_(allocator), device_(device) {}

Model::~Model() {
    if (vertexBuffer_.buffer) {
        vmaDestroyBuffer(allocator_, static_cast<VkBuffer>(vertexBuffer_.buffer), vertexBuffer_.allocation);
    }
    if (indexBuffer_.buffer) {
        vmaDestroyBuffer(allocator_, static_cast<VkBuffer>(indexBuffer_.buffer), indexBuffer_.allocation);
    }
    // Textures are owned by shared_ptr inside each Material — destroyed automatically.
}

void Model::setGpuResources(vk::Buffer vertexBuf, VmaAllocation vertexAlloc,
                             vk::Buffer indexBuf,  VmaAllocation indexAlloc,
                             uint32_t indexCount) {
    vertexBuffer_ = { vertexBuf, vertexAlloc };
    indexBuffer_  = { indexBuf,  indexAlloc  };
    indexCount_   = indexCount;
}

void Model::addMaterial(const Material &material) {
    materials_.push_back(material);
}

void Model::setSubmeshes(std::vector<Submesh> submeshes) {
    submeshes_ = std::move(submeshes);
}
