//
// Created by johnny on 2/27/26.
//
#pragma once

#include "renderer/renderer.hpp"
#include "scene/TextureManager.hpp"

#include <unordered_map>
#include <memory>
#include <string>
#include "vulkan/VulkanContext.hpp"

class Mesh;
struct Vertex;

class AssetManager {
public:
    AssetManager(VulkanContext &context, vk::CommandPool transferPool, TextureManager &textureManager,
                 Renderer &renderer)
        : context_(context), transferPool_(transferPool), textureManager_(textureManager), renderer_(renderer) {
    }

    /** @brief Loads a model from a glTF/GLB file, assembles materials, and caches it. */
    std::shared_ptr<Mesh> loadModel(const std::string &path);

    /** @brief Returns a cached unit sphere (generated procedurally on first call). */
    std::shared_ptr<Mesh> getSharedSphere();

    void clearCache() {
        modelCache_.clear();
        sharedSphere_ = nullptr;
    }

private:
    /** @brief Uploads vertex/index data to GPU and calls mesh->setGpuResources(). */
    void uploadMeshDataToGPU(std::shared_ptr<Mesh> mesh,
                              const std::vector<Vertex> &vertices,
                              const std::vector<uint32_t> &indices);

    VulkanContext &context_;
    vk::CommandPool transferPool_;
    TextureManager &textureManager_;
    Renderer &renderer_;

    std::unordered_map<std::string, std::shared_ptr<Mesh>> modelCache_;
    std::shared_ptr<Mesh> sharedSphere_ = nullptr;
};
