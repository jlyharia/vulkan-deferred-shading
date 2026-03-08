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

class Model;

class AssetManager {
public:
    // Pass the command pool from the renderer during init
    AssetManager(VulkanContext &context, vk::CommandPool transferPool, TextureManager& textureManager, Renderer& renderer)
        : context_(context), transferPool_(transferPool), textureManager_(textureManager), renderer_(renderer) {
    }

    // Return shared_ptr to match your RenderObject migration
    std::shared_ptr<Model> loadModel(const std::string &path);

    void clearCache() { modelCache_.clear(); }

private:
    VulkanContext &context_;
    vk::CommandPool transferPool_;
    TextureManager &textureManager_;
    Renderer &renderer_;
    // Store as shared_ptr so RenderObjects can "own" their reference
    std::unordered_map<std::string, std::shared_ptr<Model>> modelCache_;
};