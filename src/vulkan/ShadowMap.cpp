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
    depth_ = vk_util::AttachmentImage::create(
        context_.getVmaAllocator(),
        context_.getDevice(),
        width, height,
        DEPTH_FORMAT,
        vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eDepth);
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
                       .setCompareEnable(true)                        // enable hardware PCF
                       .setCompareOp(vk::CompareOp::eLess)            // frag depth < map depth → lit; default is eNever (always shadowed)
                       .setMipmapMode(vk::SamplerMipmapMode::eNearest);

    sampler_ = context_.getDevice().createSampler(samplerInfo);
}

void ShadowMap::cleanup() {
    depth_.cleanup(context_.getDevice(), context_.getVmaAllocator());
}
