//
// Created by johnny on 3/3/26.
//

#pragma once
#include "common/VulkanInclude.hpp"
#include "Texture.hpp"

#include <memory>
#include <string>
#include <unordered_map>

class VulkanContext;

/**
 * @class TextureManager
 * @brief Centralized resource controller for Vulkan texture assets.("VRAM Vault")
 * * The TextureManager acts as both a Resource Cache and a Factory. It ensures
 * optimal VRAM usage by deduplicating textures; if multiple models request
 * the same file, they receive a shared reference to the existing GPU resource.
 * * Key Features:
 * - Automatic Deduplication: Uses a URI-based cache to prevent redundant uploads.
 * - Lifetime Management: Leverages std::shared_ptr to automate GPU memory cleanup.
 * - Fallback System: Provides "Safe" textures (White, Flat Normal, Black) for
 * models with missing maps.
 * - GPU Pipeline: Encapsulates staging buffers, layout transitions, and mipmap
 * generation.
 */
class TextureManager {
public:
    /**
     * @brief Initializes the manager and pre-allocates fallback textures.
     * @param context Reference to the core VulkanContext for device and allocator access.
     */
    explicit TextureManager(VulkanContext &context);

    /**
         * @brief Ensures cleanup happens if not called manually.
         * Note: Requires device.waitIdle() before destruction.
         */
    ~TextureManager();
    /**
     * @brief THE NEW PURE INTERFACE
     * This replaces loadTextureFromGltf. The AssetManager handles the
     * glTF parsing and passes the raw buffer here.
     */
    std::shared_ptr<Texture> getOrCreateTexture(
        const std::string &key,
        const unsigned char *pixelData,
        uint32_t width,
        uint32_t height,
        vk::Format format
        );


    /**
     * @brief Manually clears the internal cache.
     * * Note: Resources currently held by Models/Materials will remain alive
     * until those objects are destroyed due to shared_ptr reference counting.
     */
    void clearCache();

    /**
     * @brief Forcibly destroys all Vulkan handles owned by the manager.
     * Should be called during application shutdown after device.waitIdle().
     */
    // todo put in Call cleanup() manually during your application's shutdown() sequence, and let the destructor be the backup.
    void cleanup();

    /** @return A shared, pre-configured linear sampler with 16x Anisotropy. */
    [[nodiscard]] vk::Sampler getDefaultSampler() const { return defaultSampler_; }

    /// @name Fallback Accessors
    /// These provide valid textures for materials that lack specific maps.
    /// @{
    [[nodiscard]] std::shared_ptr<Texture> getWhiteFallback() const { return whiteFallback_; }
    [[nodiscard]] std::shared_ptr<Texture> getFlatNormalFallback() const { return normalFallback_; }
    [[nodiscard]] std::shared_ptr<Texture> getBlackFallback() const { return blackFallback_; }
    /// @}

private:
    VulkanContext &context_;

    /** * @brief The Internal Cache.
     * Key: Unique file URI or name.
     * Value: Shared pointer maintaining the GPU resource lifetime.
     */
    std::unordered_map<std::string, std::shared_ptr<Texture>> textureCache_;

    /** @brief Shared sampler to avoid hitting Vulkan hardware limits. */
    vk::Sampler defaultSampler_;

    /** @brief Creates the shared sampler with optimal PBR settings. */
    void createDefaultSampler();

    /** * @brief Internal helper to create 1x1 fallback textures.
     * @param pixelData Hexadecimal color value (e.g., 0xFFFFFFFF for white).
     */
    std::shared_ptr<Texture> createSinglePixelTexture(uint32_t pixelData, vk::Format format, std::string name);

    /** * @brief Executes a GPU blit chain to generate mips and transitions to ShaderReadOnly.
     */
    void generateMipmaps(vk::Image image, vk::Format format, int32_t texWidth, int32_t texHeight,
                         uint32_t mipLevels) const;

    /**
     * @brief The primary entry point for loading textures from glTF data.
     * * Checks the internal cache using the glTF Image URI. If not found, it
     * performs a GPU upload and generates mipmaps.
     * * @param gltfImage The image data structure from tiny_gltf.
     * @param isColor Set to true for Albedo (sRGB), false for Data maps (Linear).
     * @return A shared pointer to the managed Texture resource.
     */
    // std::shared_ptr<Texture> loadTextureFromGltf(const tinygltf::Image &gltfImage, bool isColor);

    // Managed Fallback Storage
    std::shared_ptr<Texture> whiteFallback_;
    std::shared_ptr<Texture> normalFallback_;
    std::shared_ptr<Texture> blackFallback_;
};