//
// Created by johnny on 3/3/26.
//

#pragma once
#include "common/VulkanInclude.hpp"
#include "Texture.hpp"

#include <deque>
#include <memory>
#include <tiny_gltf.h>
class VulkanContext;

class TextureManager {
public:
    explicit TextureManager(VulkanContext &context);
    ~TextureManager() { cleanup(); }

    // The main function GltfLoader will call
    Texture loadTextureFromGltf(const tinygltf::Image &gltfImage, bool isColor);

    // Cleanup all textures at once
    void cleanup();
    [[nodiscard]] vk::Sampler getDefaultSampler() const { return defaultSampler_; }

    // Fallbacks for the "Arches" and "Lion" issues
    Texture &getWhiteFallback() const { return *whiteFallback_; }
    Texture &getFlatNormalFallback() const { return *normalFallback_; }
    Texture &getBlackFallback() const { return *blackFallback_; }

private:
    VulkanContext &context_;
    // We store these to destroy them later
    // std::deque<Texture> loadedTextures_;
    vk::Sampler defaultSampler_;
    void createDefaultSampler();
    Texture createSinglePixelTexture(uint32_t pixelData, vk::Format format, std::string name) const;

    // Fallback storage
    std::unique_ptr<Texture> whiteFallback_;
    std::unique_ptr<Texture> normalFallback_;
    std::unique_ptr<Texture> blackFallback_; // For Metallic-Roughness (0,0,0,1)
};