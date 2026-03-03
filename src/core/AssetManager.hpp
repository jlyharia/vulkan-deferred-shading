//
// Created by johnny on 2/27/26.
//
#pragma once

#include <unordered_map>
#include <memory>
#include <string>
#include "vulkan/VulkanContext.hpp"

class Model;

class AssetManager {
public:
    // Pass the command pool from the renderer during init
    AssetManager(VulkanContext &context, vk::CommandPool transferPool)
        : context_(context), transferPool_(transferPool) {
    }

    // Return shared_ptr to match your RenderObject migration
    std::shared_ptr<Model> loadModel(const std::string &path);

    void clearCache() { modelCache_.clear(); }

private:
    VulkanContext &context_;
    vk::CommandPool transferPool_;
    // Store as shared_ptr so RenderObjects can "own" their reference
    std::unordered_map<std::string, std::shared_ptr<Model>> modelCache_;
};