//
// Created by johnny on 2/27/26.
//

#include "AssetManager.hpp"
#include "scene/Mesh.hpp"
#include "scene/MeshUtils.hpp"
#include "assets/GltfLoader.hpp"
#include "vulkan/VulkanUtils.hpp"
#include <iostream>

std::shared_ptr<Mesh> AssetManager::loadModel(const std::string &path) {
    if (modelCache_.contains(path)) return modelCache_[path];

    GltfLoader loader(textureManager_);
    const bool isBinary = path.ends_with(".glb");
    auto data = loader.loadFromFile(path, isBinary);

    if (!data.success) {
        std::cerr << "[AssetManager] Failed to load: " << path << std::endl;
        return nullptr;
    }

    auto mesh = std::make_shared<Mesh>(context_.getVmaAllocator(), context_.getDevice());
    uploadMeshDataToGPU(mesh, data.vertices, data.indices);
    mesh->setSubmeshes(std::move(data.primitives));

    for (auto const &[matIdx, info] : data.materials) {
        Material mat;
        mat.name            = info.name;
        mat.baseColorFactor = info.baseColorFactor;
        mat.metallicFactor  = info.metallicFactor;
        mat.roughnessFactor = info.roughnessFactor;
        mat.doubleSided     = info.doubleSided;
        mat.alphaCutoff     = info.alphaCutoff;
        mat.isTransparent   = info.isTransparent;

        mat.albedoTexture            = (info.baseColorIdx >= 0)         ? data.textures[info.baseColorIdx]         : textureManager_.getWhiteFallback();
        mat.normalTexture            = (info.normalIdx >= 0)            ? data.textures[info.normalIdx]            : textureManager_.getFlatNormalFallback();
        mat.metallicRoughnessTexture = (info.metallicRoughnessIdx >= 0) ? data.textures[info.metallicRoughnessIdx] : textureManager_.getBlackFallback();

        mat.textureSet = renderer_.createTextureDescriptorSet(
            mat.albedoTexture->imageView,
            mat.normalTexture->imageView,
            mat.metallicRoughnessTexture->imageView,
            textureManager_.getDefaultSampler());

        mesh->addMaterial(mat);
    }

    modelCache_[path] = mesh;
    return mesh;
}

std::shared_ptr<Mesh> AssetManager::getSharedSphere() {
    if (sharedSphere_) return sharedSphere_;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    MeshUtils::generateSphere(1.0f, 32, 32, vertices, indices);

    Material lightMat;
    lightMat.name                     = "Procedural_Light_Mat";
    lightMat.baseColorFactor          = glm::vec4(1.0f); // tint comes from MeshInstance::color
    lightMat.unlit                    = true;
    lightMat.albedoTexture            = textureManager_.getWhiteFallback();
    lightMat.normalTexture            = textureManager_.getFlatNormalFallback();
    lightMat.metallicRoughnessTexture = textureManager_.getBlackFallback();
    lightMat.textureSet = renderer_.createTextureDescriptorSet(
        lightMat.albedoTexture->imageView,
        lightMat.normalTexture->imageView,
        lightMat.metallicRoughnessTexture->imageView,
        textureManager_.getDefaultSampler());

    auto mesh = std::make_shared<Mesh>(context_.getVmaAllocator(), context_.getDevice());
    uploadMeshDataToGPU(mesh, vertices, indices);

    Submesh submesh{};
    submesh.firstIndex    = 0;
    submesh.indexCount    = static_cast<uint32_t>(indices.size());
    submesh.materialIndex = 0;
    mesh->setSubmeshes({submesh});
    mesh->addMaterial(lightMat);

    sharedSphere_ = mesh;
    return sharedSphere_;
}

void AssetManager::uploadMeshDataToGPU(std::shared_ptr<Mesh> mesh,
                                       const std::vector<Vertex> &vertices,
                                       const std::vector<uint32_t> &indices) {
    vk::DeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();
    vk::DeviceSize indexBufferSize  = sizeof(uint32_t) * indices.size();

    // 1. Create Staging Buffers (CPU Visible)
    vk::Buffer stagingVBuf, stagingIBuf;
    VmaAllocation stagingVAlloc, stagingIAlloc;

    vk_util::createBuffer(context_.getVmaAllocator(), vertexBufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                          VMA_MEMORY_USAGE_CPU_ONLY, stagingVBuf, stagingVAlloc);
    vk_util::createBuffer(context_.getVmaAllocator(), indexBufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                          VMA_MEMORY_USAGE_CPU_ONLY, stagingIBuf, stagingIAlloc);

    // 2. Map and Copy
    void *vData, *iData;
    vmaMapMemory(context_.getVmaAllocator(), stagingVAlloc, &vData);
    memcpy(vData, vertices.data(), vertexBufferSize);
    vmaUnmapMemory(context_.getVmaAllocator(), stagingVAlloc);

    vmaMapMemory(context_.getVmaAllocator(), stagingIAlloc, &iData);
    memcpy(iData, indices.data(), indexBufferSize);
    vmaUnmapMemory(context_.getVmaAllocator(), stagingIAlloc);

    // 3. Create GPU-Only Buffers
    vk::Buffer gpuVBuf, gpuIBuf;
    VmaAllocation gpuVAlloc, gpuIAlloc;

    vk_util::createBuffer(context_.getVmaAllocator(), vertexBufferSize,
                          vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                          VMA_MEMORY_USAGE_GPU_ONLY, gpuVBuf, gpuVAlloc);
    vk_util::createBuffer(context_.getVmaAllocator(), indexBufferSize,
                          vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                          VMA_MEMORY_USAGE_GPU_ONLY, gpuIBuf, gpuIAlloc);

    // 4. Record and Submit Transfer Commands
    vk_util::copyBuffer(context_.getDevice(), transferPool_, context_.getGraphicsQueue(),
                        stagingVBuf, gpuVBuf, vertexBufferSize);
    vk_util::copyBuffer(context_.getDevice(), transferPool_, context_.getGraphicsQueue(),
                        stagingIBuf, gpuIBuf, indexBufferSize);

    // 5. Clean up Staging
    vmaDestroyBuffer(context_.getVmaAllocator(), stagingVBuf, stagingVAlloc);
    vmaDestroyBuffer(context_.getVmaAllocator(), stagingIBuf, stagingIAlloc);

    // 6. Hand GPU handles to the Mesh
    mesh->setGpuResources(gpuVBuf, gpuVAlloc, gpuIBuf, gpuIAlloc,
                          static_cast<uint32_t>(indices.size()));
}
