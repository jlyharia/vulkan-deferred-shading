//
// Created by johnny on 3/3/26.
//

#include "TextureManager.hpp"

#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanUtils.hpp" // Your namespace vk_util
#include <iostream>

TextureManager::TextureManager(VulkanContext &context) : context_(context) {
    createDefaultSampler();
}

Texture TextureManager::loadTextureFromGltf(const tinygltf::Image &gltfImage) {
    // 1. Calculate size and validate
    vk::DeviceSize imageSize = gltfImage.width * gltfImage.height * 4;
    if (gltfImage.component != 4) {
        // Most glTF loaders (stb) output RGBA, but just in case:
        throw std::runtime_error("TextureManager only supports 4-component RGBA textures for now.");
    }

    // 2. Create Staging Buffer
    vk::Buffer stagingBuffer;
    VmaAllocation stagingAllocation;
    vk_util::createBuffer(
        context_.getVmaAllocator(),
        imageSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_CPU_ONLY,
        stagingBuffer,
        stagingAllocation
        );

    // 3. Map memory and copy pixels
    void *data;
    vmaMapMemory(context_.getVmaAllocator(), stagingAllocation, &data);
    memcpy(data, gltfImage.image.data(), static_cast<size_t>(imageSize));
    vmaUnmapMemory(context_.getVmaAllocator(), stagingAllocation);

    // 4. Create Final GPU Image
    Texture texture;
    texture.width = gltfImage.width;
    texture.height = gltfImage.height;
    texture.name = gltfImage.name;
    texture.sampler = getDefaultSampler();
    // Use sRGB for base color textures so gamma correction is handled by hardware
    vk::Format format = vk::Format::eR8G8B8A8Srgb;

    vk_util::createImage(
        context_.getVmaAllocator(),
        texture.width,
        texture.height,
        format,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        VMA_MEMORY_USAGE_GPU_ONLY,
        texture.image,
        texture.allocation
        );

    // 5. Execute GPU Transfer
    // Move to Transfer Destination layout
    vk_util::transitionImageLayout(
        context_.getDevice(), context_.getTransferCommandPool(), context_.getGraphicsQueue(),
        texture.image, format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal
        );

    // Copy buffer to image
    vk_util::copyBufferToImage(
        context_.getDevice(), context_.getTransferCommandPool(), context_.getGraphicsQueue(),
        stagingBuffer, texture.image, texture.width, texture.height
        );

    // Move to Shader Read layout
    vk_util::transitionImageLayout(
        context_.getDevice(), context_.getTransferCommandPool(), context_.getGraphicsQueue(),
        texture.image, format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal
        );

    // 6. Cleanup Staging Resources
    vmaDestroyBuffer(context_.getVmaAllocator(), stagingBuffer, stagingAllocation);

    // 7. Create View
    texture.imageView = vk_util::createImageView(context_.getDevice(), texture.image, format);

    // Store for mass cleanup
    loadedTextures_.push_back(texture);

    std::cout << "[TextureManager] Loaded texture: " << texture.width << "x" << texture.height << std::endl;
    return texture;
}


void TextureManager::cleanup() {
    for (auto &tex : loadedTextures_) {
        if (tex.imageView) context_.getDevice().destroyImageView(tex.imageView);
        if (tex.image) vmaDestroyImage(context_.getVmaAllocator(), tex.image, tex.allocation);
        // DO NOT destroy tex.sampler here if it's the shared defaultSampler_!
    }
    loadedTextures_.clear();

    if (defaultSampler_) {
        context_.getDevice().destroySampler(defaultSampler_);
        defaultSampler_ = nullptr;
    }
}


void TextureManager::createDefaultSampler() {
    const vk::SamplerCreateInfo samplerInfo =
        vk::SamplerCreateInfo()
        .setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setAddressModeU(vk::SamplerAddressMode::eRepeat) // Sponza floors repeat!
        .setAddressModeV(vk::SamplerAddressMode::eRepeat)
        .setAddressModeW(vk::SamplerAddressMode::eRepeat)
        .setAnisotropyEnable(VK_TRUE) // Makes textures sharp at angles
        .setMaxAnisotropy(
            context_.getPhysicalDevice().getProperties().limits.
                     maxSamplerAnisotropy)
        .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
        .setUnnormalizedCoordinates(VK_FALSE)
        .setCompareEnable(VK_FALSE)
        .setMipmapMode(vk::SamplerMipmapMode::eLinear);

    defaultSampler_ = context_.getDevice().createSampler(samplerInfo);
}

