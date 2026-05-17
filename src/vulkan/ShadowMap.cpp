//
// Created by johnny on 4/6/26.
//

#include "ShadowMap.hpp"
#include "VulkanContext.hpp"

ShadowMap::ShadowMap(VulkanContext &context, uint32_t width, uint32_t height)
    : context_(context) {
    createImages(width, height);
    createSampler();
}

ShadowMap::~ShadowMap() {
    cleanup();
    context_.getDevice().destroySampler(sampler_);
}

void ShadowMap::recreate(uint32_t width, uint32_t height) {
    cleanup();
    createImages(width, height);
}

void ShadowMap::createImages(uint32_t width, uint32_t height) {
    extent_ = vk::Extent2D{width, height};
    auto device = context_.getDevice();
    auto allocator = context_.getVmaAllocator();

    auto imageInfo = vk::ImageCreateInfo()
                     .setImageType(vk::ImageType::e2D)
                     .setFormat(DEPTH_FORMAT)
                     .setExtent({width, height, 1})
                     .setMipLevels(1)
                     .setArrayLayers(engineConfig::NUM_CASCADES)
                     .setSamples(vk::SampleCountFlagBits::e1)
                     .setTiling(vk::ImageTiling::eOptimal)
                     .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment |
                               vk::ImageUsageFlagBits::eSampled)
                     .setSharingMode(vk::SharingMode::eExclusive)
                     .setInitialLayout(vk::ImageLayout::eUndefined);

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkImage rawImage{};
    if (vmaCreateImage(allocator, reinterpret_cast<const VkImageCreateInfo *>(&imageInfo),
                       &allocInfo, &rawImage, &depthAlloc_, nullptr) != VK_SUCCESS)
        throw std::runtime_error("ShadowMap: failed to create cascade depth image");
    depthImage_ = rawImage;

    // Full array view — sampler2DArrayShadow in lighting.frag
    depthArrayView_ = device.createImageView(
        vk::ImageViewCreateInfo()
        .setImage(depthImage_)
        .setViewType(vk::ImageViewType::e2DArray)
        .setFormat(DEPTH_FORMAT)
        .setSubresourceRange(vk::ImageSubresourceRange()
                             .setAspectMask(vk::ImageAspectFlagBits::eDepth)
                             .setBaseMipLevel(0)
                             .setLevelCount(1)
                             .setBaseArrayLayer(0)
                             .setLayerCount(engineConfig::NUM_CASCADES)));

    // Per-layer views — depth attachment for each cascade in DirShadowPass
    for (uint32_t i = 0; i < engineConfig::NUM_CASCADES; i++) {
        layerViews_[i] = device.createImageView(
            vk::ImageViewCreateInfo()
            .setImage(depthImage_)
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(DEPTH_FORMAT)
            .setSubresourceRange(vk::ImageSubresourceRange()
                                 .setAspectMask(vk::ImageAspectFlagBits::eDepth)
                                 .setBaseMipLevel(0)
                                 .setLevelCount(1)
                                 .setBaseArrayLayer(i)
                                 .setLayerCount(1)));
    }
}

void ShadowMap::createSampler() {
    // eClampToEdge: fragments outside the shadow frustum sample the edge depth,
    // avoiding wrap-around artifacts at shadow map borders.
    auto samplerInfo = vk::SamplerCreateInfo()
                       .setMagFilter(vk::Filter::eLinear)
                       .setMinFilter(vk::Filter::eLinear)
                       .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                       .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                       .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                       .setAnisotropyEnable(false)
                       .setUnnormalizedCoordinates(false)
                       .setCompareEnable(true) // enable hardware PCF
                       .setCompareOp(vk::CompareOp::eGreaterOrEqual) // reverse-Z: frag >= map → lit
                       .setMipmapMode(vk::SamplerMipmapMode::eNearest);

    sampler_ = context_.getDevice().createSampler(samplerInfo);
}

void ShadowMap::cleanup() {
    auto device = context_.getDevice();
    auto allocator = context_.getVmaAllocator();

    for (auto &view : layerViews_)
        if (view) {
            device.destroyImageView(view);
            view = nullptr;
        }

    if (depthArrayView_) {
        device.destroyImageView(depthArrayView_);
        depthArrayView_ = nullptr;
    }
    if (depthImage_) {
        vmaDestroyImage(allocator, depthImage_, depthAlloc_);
        depthImage_ = nullptr;
        depthAlloc_ = nullptr;
    }
}