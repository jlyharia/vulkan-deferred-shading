//
// Created by johnny on 3/3/26.
//

#pragma once
#include "common/VulkanInclude.hpp"
#include "Texture.hpp"
#include <tiny_gltf.h>
class VulkanContext;

class TextureManager {
public:
    explicit TextureManager(VulkanContext &context);
    ~TextureManager() { cleanup(); }

    // The main function GltfLoader will call
    Texture loadTextureFromGltf(const tinygltf::Image &gltfImage);

    // Cleanup all textures at once
    void cleanup();
    [[nodiscard]] vk::Sampler getDefaultSampler() const { return defaultSampler_; }

private:
    VulkanContext &context_;
    // We store these to destroy them later
    std::vector<Texture> loadedTextures_;
    vk::Sampler defaultSampler_;
    void createDefaultSampler();
};