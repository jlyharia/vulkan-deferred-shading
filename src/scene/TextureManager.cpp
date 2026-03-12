//
// Created by johnny on 3/3/26.
//

#include "TextureManager.hpp"

#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanUtils.hpp" // Your namespace vk_util
#include <iostream>

TextureManager::TextureManager(VulkanContext &context) : context_(context) {

    createDefaultSampler();

    // 1. Allocate the unique_ptrs
    whiteFallback_ = std::make_unique<Texture>();
    normalFallback_ = std::make_unique<Texture>();
    blackFallback_ = std::make_unique<Texture>(); // Added this!

    // 2. Initialize with names for the Vulkan Debugger
    // 0xFFFFFFFF = Pure White (Albedo)
    *whiteFallback_ = createSinglePixelTexture(0xFFFFFFFF, vk::Format::eR8G8B8A8Srgb, "White_Fallback");

    // 0xFFFF8080 = Flat Normal (Tangent Space: 128, 128, 255)
    *normalFallback_ = createSinglePixelTexture(0xFFFF8080, vk::Format::eR8G8B8A8Unorm, "Flat_Normal_Fallback");

    // 0xFF000000 = Black (Roughness: 0, Metallic: 0)
    *blackFallback_ = createSinglePixelTexture(0xFF000000, vk::Format::eR8G8B8A8Unorm, "Black_PBR_Fallback");
}

Texture TextureManager::loadTextureFromGltf(const tinygltf::Image &gltfImage, const bool isColor) const {
    uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(gltfImage.width, gltfImage.height)))) + 1;
    // 1. Calculate size and validate
    vk::DeviceSize imageSize = gltfImage.width * gltfImage.height * 4;

    // Safety check: ensure we have 4 components (RGBA)
    if (gltfImage.component != 4) {
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

    // --- THE PBR CHANGE: Format Selection ---
    // sRGB for Albedo/Diffuse colors.
    // Unorm for Linear data like Normal, Metallic, or Roughness maps.
    vk::Format format = isColor ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;

    // 4. Create Final GPU Image
    Texture texture;
    texture.width = gltfImage.width;
    texture.height = gltfImage.height;
    texture.name = gltfImage.name;
    texture.format = format; // Store this in your updated struct for debugging
    texture.mipLevels = mipLevels;

    vk_util::createImage(
        context_.getVmaAllocator(),
        texture.width,
        texture.height,
        format,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled,
        VMA_MEMORY_USAGE_GPU_ONLY,
        texture.image,
        texture.allocation,
        texture.mipLevels
        );

    // 5. Execute GPU Transfer (Standard transition logic)
    vk_util::transitionImageLayout(
        context_.getDevice(), context_.getTransferCommandPool(), context_.getGraphicsQueue(),
        texture.image, format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
        mipLevels
        );

    vk_util::copyBufferToImage(
        context_.getDevice(), context_.getTransferCommandPool(), context_.getGraphicsQueue(),
        stagingBuffer, texture.image, texture.width, texture.height
        );

    // REMOVED: transitionImageLayout to ShaderReadOnly
    // Because generateMipmaps handles the transition to ShaderReadOnly for us!
    // vk_util::transitionImageLayout(
    //     context_.getDevice(), context_.getTransferCommandPool(), context_.getGraphicsQueue(),
    //     texture.image, format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal
    //     );

    generateMipmaps(texture.image, format, texture.width, texture.height, mipLevels);
    // 6. Cleanup Staging Resources
    vmaDestroyBuffer(context_.getVmaAllocator(), stagingBuffer, stagingAllocation);

    // 7. Create View
    texture.imageView = vk_util::createImageView(context_.getDevice(), texture.image, format,
                                                 vk::ImageAspectFlagBits::eColor, mipLevels);

    // Store for mass cleanup in TextureManager::cleanup()
    // loadedTextures_.push_back(texture);

    std::cout << "[TextureManager] Loaded " << (isColor ? "Color" : "Data")
        << " texture: " << texture.name << " (" << texture.width << "x" << texture.height << ")" << std::endl;

    return texture;
}

void TextureManager::cleanup() {
    // Helper lambda to avoid repeating the destruction logic
    auto destroyTex = [this](Texture &tex) {
        if (tex.imageView) {
            context_.getDevice().destroyImageView(tex.imageView);
            tex.imageView = nullptr;
        }
        if (tex.image) {
            vmaDestroyImage(context_.getVmaAllocator(), tex.image, tex.allocation);
            tex.image = nullptr;
            tex.allocation = nullptr;
        }
    };

    // 2. Clean up Fallbacks (Crucial: otherwise these leak every time you restart)
    if (whiteFallback_) {
        destroyTex(*whiteFallback_);
    }
    if (normalFallback_) {
        destroyTex(*normalFallback_);
    }

    if (blackFallback_) {
        destroyTex(*blackFallback_);
    }

    // 3. Clean up the Sampler
    if (defaultSampler_) {
        context_.getDevice().destroySampler(defaultSampler_);
        defaultSampler_ = nullptr;
    }

    std::cout << "[TextureManager] Cleanup complete. All Vulkan resources freed." << std::endl;
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
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setMinLod(0.0f)// change to 8.0 to check if mipmap is working
        .setMaxLod(VK_LOD_CLAMP_NONE); // or 16.0f Use a high enough number to cover all possible mips (16 is plenty for 4K);

    defaultSampler_ = context_.getDevice().createSampler(samplerInfo);
}


Texture TextureManager::createSinglePixelTexture(uint32_t pixelData, vk::Format format, std::string name) const {
    Texture tex;
    tex.width = 1;
    tex.height = 1;
    tex.name = std::move(name);
    tex.format = format;

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
                         VMA_MEMORY_USAGE_GPU_ONLY, tex.image, tex.allocation);

    vk_util::transitionImageLayout(context_.getDevice(), context_.getTransferCommandPool(), context_.getGraphicsQueue(),
                                   tex.image, format, vk::ImageLayout::eUndefined,
                                   vk::ImageLayout::eTransferDstOptimal);

    vk_util::copyBufferToImage(context_.getDevice(), context_.getTransferCommandPool(), context_.getGraphicsQueue(),
                               staging, tex.image, 1, 1);

    vk_util::transitionImageLayout(context_.getDevice(), context_.getTransferCommandPool(), context_.getGraphicsQueue(),
                                   tex.image, format, vk::ImageLayout::eTransferDstOptimal,
                                   vk::ImageLayout::eShaderReadOnlyOptimal);

    vmaDestroyBuffer(context_.getVmaAllocator(), staging, stagingAlloc);
    tex.imageView = vk_util::createImageView(context_.getDevice(), tex.image, format);

    return tex;
}

void TextureManager::generateMipmaps(const vk::Image image,
                                     vk::Format format,
                                     const int32_t texWidth,
                                     const int32_t texHeight,
                                     const uint32_t mipLevels) const {
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
        // Transition previous level to TransferSrc
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {},
                                      nullptr, nullptr, barrier);

        // Blit from previous level to current level
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

        // Transition previous level to ShaderReadOnly
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

    // Transition the last mip level to ShaderReadOnly
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