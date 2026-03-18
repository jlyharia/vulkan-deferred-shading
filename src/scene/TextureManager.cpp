//
// Created by johnny on 3/3/26.
//

#include "TextureManager.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanUtils.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

TextureManager::TextureManager(VulkanContext &context) : context_(context) {
    createDefaultSampler();

    // Initialize Fallbacks with specific hex colors (ABGR format for easy memcpy)
    // White: 0xFFFFFFFF
    whiteFallback_ = createSinglePixelTexture(0xFFFFFFFF, vk::Format::eR8G8B8A8Srgb, "White_Fallback");
    // Flat Normal (Blueish): 0xFFFF8080 (Matches roughly 0.5, 0.5, 1.0)
    normalFallback_ = createSinglePixelTexture(0xFFFF8080, vk::Format::eR8G8B8A8Unorm, "Flat_Normal_Fallback");
    // Black (PBR Metal/Roughness/Occlusion): 0xFF000000
    blackFallback_ = createSinglePixelTexture(0xFF000000, vk::Format::eR8G8B8A8Unorm, "Black_PBR_Fallback");
}

TextureManager::~TextureManager() {
    cleanup();
}

/**
 * THE PURE INTERFACE:
 * This is the new "Workhorse" of the class. It doesn't know about glTF.
 */
std::shared_ptr<Texture> TextureManager::getOrCreateTexture(
    const std::string &key,
    const unsigned char *pixelData,
    uint32_t width,
    uint32_t height,
    vk::Format format) {
    // 1. Cache Check
    if (textureCache_.contains(key)) {
        return textureCache_[key];
    }

    // 2. Setup Texture metadata
    auto texture = std::make_shared<Texture>();
    texture->device = context_.getDevice();
    texture->allocator = context_.getVmaAllocator();
    texture->width = width;
    texture->height = height;
    texture->name = key;
    texture->format = format;
    texture->mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

    vk::DeviceSize imageSize = width * height * 4;

    // 3. Create Staging Buffer
    vk::Buffer stagingBuffer;
    VmaAllocation stagingAllocation;
    vk_util::createBuffer(context_.getVmaAllocator(), imageSize,
                          vk::BufferUsageFlagBits::eTransferSrc,
                          VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingAllocation);

    // Map and Copy
    void *data;
    vmaMapMemory(context_.getVmaAllocator(), stagingAllocation, &data);
    memcpy(data, pixelData, static_cast<size_t>(imageSize));
    vmaUnmapMemory(context_.getVmaAllocator(), stagingAllocation);

    // 4. Create GPU Image
    vk_util::createImage(
        context_.getVmaAllocator(), texture->width, texture->height, format, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled,
        VMA_MEMORY_USAGE_GPU_ONLY, texture->image, texture->allocation, texture->mipLevels);

    // 5. GPU Transfer
    vk_util::transitionImageLayout(context_.getDevice(), context_.getTransferCommandPool(), context_.getGraphicsQueue(),
                                   texture->image, format, vk::ImageLayout::eUndefined,
                                   vk::ImageLayout::eTransferDstOptimal, texture->mipLevels);

    vk_util::copyBufferToImage(context_.getDevice(), context_.getTransferCommandPool(), context_.getGraphicsQueue(),
                               stagingBuffer, texture->image, texture->width, texture->height);

    // 6. Finalize (Mips & Transition to ShaderReadOnly)
    generateMipmaps(texture->image, format, texture->width, texture->height, texture->mipLevels);

    // Cleanup Staging
    vmaDestroyBuffer(context_.getVmaAllocator(), stagingBuffer, stagingAllocation);

    // Create View
    texture->imageView = vk_util::createImageView(context_.getDevice(), texture->image, format,
                                                  vk::ImageAspectFlagBits::eColor, texture->mipLevels);

    // 7. Store in Cache
    textureCache_[key] = texture;
    std::cout << "[TextureManager] VRAM Upload Successful: " << key << " (" << width << "x" << height << ")" <<
        std::endl;

    return texture;
}

void TextureManager::clearCache() {
    textureCache_.clear();
}

void TextureManager::cleanup() {
    // Releasing shared_ptrs triggers Texture::~Texture() on last ref, which
    // destroys the GPU resources. No manual vkDestroy needed here.
    textureCache_.clear();
    whiteFallback_.reset();
    normalFallback_.reset();
    blackFallback_.reset();

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
        .setAddressModeU(vk::SamplerAddressMode::eRepeat)
        .setAddressModeV(vk::SamplerAddressMode::eRepeat)
        .setAddressModeW(vk::SamplerAddressMode::eRepeat)
        .setAnisotropyEnable(VK_TRUE)
        .setMaxAnisotropy(context_.getPhysicalDevice().getProperties().limits.maxSamplerAnisotropy)
        .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
        .setUnnormalizedCoordinates(VK_FALSE)
        .setCompareEnable(VK_FALSE)
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setMinLod(0.0f)
        .setMaxLod(VK_LOD_CLAMP_NONE);

    defaultSampler_ = context_.getDevice().createSampler(samplerInfo);
}

std::shared_ptr<Texture> TextureManager::createSinglePixelTexture(uint32_t pixelData, vk::Format format,
                                                                  std::string name) {
    auto tex = std::make_shared<Texture>();
    tex->device = context_.getDevice();
    tex->allocator = context_.getVmaAllocator();
    tex->width = 1;
    tex->height = 1;
    tex->name = std::move(name);
    tex->format = format;
    tex->mipLevels = 1;

    vk::DeviceSize size = 4;
    vk::Buffer staging;
    VmaAllocation stagingAlloc;
    vk_util::createBuffer(context_.getVmaAllocator(), size, vk::BufferUsageFlagBits::eTransferSrc,
                          VMA_MEMORY_USAGE_CPU_ONLY, staging, stagingAlloc);

    void *data;
    vmaMapMemory(context_.getVmaAllocator(), stagingAlloc, &data);
    memcpy(data, &pixelData, 4);
    vmaUnmapMemory(context_.getVmaAllocator(), stagingAlloc);

    vk_util::createImage(context_.getVmaAllocator(), 1, 1, format, vk::ImageTiling::eOptimal,
                         vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                         VMA_MEMORY_USAGE_GPU_ONLY, tex->image, tex->allocation);

    vk_util::transitionImageLayout(context_.getDevice(), context_.getTransferCommandPool(), context_.getGraphicsQueue(),
                                   tex->image, format, vk::ImageLayout::eUndefined,
                                   vk::ImageLayout::eTransferDstOptimal);

    vk_util::copyBufferToImage(context_.getDevice(), context_.getTransferCommandPool(), context_.getGraphicsQueue(),
                               staging, tex->image, 1, 1);

    vk_util::transitionImageLayout(context_.getDevice(), context_.getTransferCommandPool(), context_.getGraphicsQueue(),
                                   tex->image, format, vk::ImageLayout::eTransferDstOptimal,
                                   vk::ImageLayout::eShaderReadOnlyOptimal);

    vmaDestroyBuffer(context_.getVmaAllocator(), staging, stagingAlloc);
    tex->imageView = vk_util::createImageView(context_.getDevice(), tex->image, format);
    return tex;
}

void TextureManager::generateMipmaps(const vk::Image image, vk::Format format, const int32_t texWidth,
                                     const int32_t texHeight, const uint32_t mipLevels) const {
    vk::FormatProperties formatProperties = context_.getPhysicalDevice().getFormatProperties(format);

    if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
        throw std::runtime_error("Texture image format does not support linear blitting!");
    }

    vk::CommandBuffer commandBuffer = vk_util::beginSingleTimeCommands(context_.getDevice(),
                                                                       context_.getTransferCommandPool());

    vk::ImageMemoryBarrier barrier{};
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {},
                                      nullptr, nullptr, barrier);

        vk::ImageBlit blit{};
        blit.srcOffsets[0] = vk::Offset3D{0, 0, 0};
        blit.srcOffsets[1] = vk::Offset3D{mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = vk::Offset3D{0, 0, 0};
        blit.dstOffsets[1] = vk::Offset3D{mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image,
                                vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);

        barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
                                      {}, nullptr, nullptr, barrier);

        if (mipWidth > 1)
            mipWidth /= 2;
        if (mipHeight > 1)
            mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {},
                                  nullptr, nullptr, barrier);

    vk_util::endSingleTimeCommands(context_.getDevice(), context_.getTransferCommandPool(), context_.getGraphicsQueue(),
                                   commandBuffer);
}