//
// Created by johnny on 2/25/26.
//

#pragma once
#include "common/VulkanInclude.hpp"

// =============================================================================
// AttachmentImage — owns a GPU render-target image (image + view + allocation).
// All three resources are always created and destroyed together.
// =============================================================================
namespace vk_util {

struct AttachmentImage {
    vk::Image image{};
    vk::ImageView view{};
    VmaAllocation allocation{};

    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(image); }

    static AttachmentImage create(
        VmaAllocator allocator,
        vk::Device device,
        uint32_t width,
        uint32_t height,
        vk::Format format,
        vk::ImageUsageFlags usage,
        vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor);

    void cleanup(vk::Device device, VmaAllocator allocator) noexcept;
};

// 1. Generic Buffer Creation (VMA)
void createBuffer(
    VmaAllocator allocator,
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    VmaMemoryUsage vmaUsage,
    vk::Buffer &buffer,
    VmaAllocation &allocation,
    VmaAllocationCreateFlags vmaFlags = 0,
    VmaAllocationInfo *outAllocInfo = nullptr);

void copyBuffer(
    vk::Device device,
    vk::CommandPool commandPool,
    vk::Queue queue,
    vk::Buffer srcBuffer,
    vk::Buffer dstBuffer,
    vk::DeviceSize size);
// 2. Generic Image Creation (VMA)
void createImage(
    VmaAllocator allocator,
    uint32_t width,
    uint32_t height,
    vk::Format format,
    vk::ImageTiling tiling,
    vk::ImageUsageFlags usage,
    VmaMemoryUsage vmaUsage,
    vk::Image &image,
    VmaAllocation &allocation,
    uint32_t mipLevels = 1
    );

// 3. Image Layout Transition (Essential for Textures)
void transitionImageLayout(
    vk::Device device,
    vk::CommandPool commandPool,
    vk::Queue graphicsQueue,
    vk::Image image,
    vk::Format format,
    vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout,
    uint32_t mipLevels = 1);

// 4. Copy Buffer to Image
void copyBufferToImage(
    vk::Device device,
    vk::CommandPool commandPool,
    vk::Queue graphicsQueue,
    vk::Buffer buffer,
    vk::Image image,
    uint32_t width,
    uint32_t height);

vk::ImageView createImageView(
    vk::Device device,
    vk::Image image,
    vk::Format format,
    vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor,
    const uint32_t mipLevels = 1
    );

// =============================================================================
// Barrier factories — common image layout transitions for the render loop.
// All return ImageMemoryBarrier2 (Vulkan 1.3 synchronization2).
// =============================================================================

// COLOR_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL (after a color attachment write)
[[nodiscard]] vk::ImageMemoryBarrier2 colorAttachmentToShaderRead(vk::Image image) noexcept;

// DEPTH_STENCIL_ATTACHMENT_OPTIMAL → DEPTH_READ_ONLY_OPTIMAL (after geometry depth write)
[[nodiscard]] vk::ImageMemoryBarrier2 depthToShaderRead(vk::Image image) noexcept;

// UNDEFINED → COLOR_ATTACHMENT_OPTIMAL (frame-start transition for color targets)
[[nodiscard]] vk::ImageMemoryBarrier2 undefinedToColorAttachment(vk::Image image) noexcept;

// COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC_KHR (end-of-frame presentation transition)
[[nodiscard]] vk::ImageMemoryBarrier2 colorAttachmentToPresent(vk::Image image) noexcept;

// UNDEFINED → DEPTH_STENCIL_ATTACHMENT_OPTIMAL (frame-start for shadow map — contents discarded, cleared on load)
[[nodiscard]] vk::ImageMemoryBarrier2 undefinedToDepthAttachment(vk::Image image) noexcept;

// DEPTH_STENCIL_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL (after shadow map write, for texture sampling)
[[nodiscard]] vk::ImageMemoryBarrier2 depthAttachmentToShaderRead(vk::Image image) noexcept;

// =============================================================================
// Descriptor write helpers
// =============================================================================

// Builds a WriteDescriptorSet for a combined-image-sampler binding.
// The caller must keep imageInfo alive until device.updateDescriptorSets returns.
[[nodiscard]] vk::WriteDescriptorSet imageSamplerWrite(
    vk::DescriptorSet set,
    uint32_t binding,
    const vk::DescriptorImageInfo &imageInfo) noexcept;

// =============================================================================
// uploadToDeviceImage — one-shot staging upload for sampled images.
// Allocates a staging buffer, uploads data, creates the GPU image,
// transitions layout to eShaderReadOnlyOptimal, and cleans up staging.
// The caller is responsible for creating the ImageView afterwards.
// Not suitable for mip-generating uploads (see TextureManager::getOrCreateTexture).
// =============================================================================
void uploadToDeviceImage(
    VmaAllocator allocator,
    vk::Device device,
    vk::CommandPool commandPool,
    vk::Queue queue,
    const void *data,
    vk::DeviceSize dataSize,
    uint32_t width,
    uint32_t height,
    vk::Format format,
    vk::Image &outImage,
    VmaAllocation &outAlloc);

// 5. One-Time Command Helper (Internal use)
vk::CommandBuffer beginSingleTimeCommands(vk::Device device, vk::CommandPool commandPool);
void endSingleTimeCommands(vk::Device device, vk::CommandPool commandPool, vk::Queue queue,
                           vk::CommandBuffer commandBuffer);

template <typename T>
void uploadToDeviceBuffer(
    const VmaAllocator allocator,
    const vk::Device device,
    const vk::Queue queue,
    const vk::CommandPool commandPool,
    const std::vector<T> &data,
    const vk::BufferUsageFlags usage,
    vk::Buffer &outBuffer,
    VmaAllocation &outAllocation) {
    const vk::DeviceSize bufferSize = sizeof(T) * data.size();

    // 1. Staging Buffer (CPU Visible)
    vk::Buffer stagingBuffer;
    VmaAllocation stagingAlloc;
    createBuffer(allocator, bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                 VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingAlloc);

    // 2. Map and Copy
    void *mappedData;
    vmaMapMemory(allocator, stagingAlloc, &mappedData);
    memcpy(mappedData, data.data(), static_cast<size_t>(bufferSize));
    vmaUnmapMemory(allocator, stagingAlloc);

    // 3. GPU Buffer (Device Local)
    createBuffer(allocator, bufferSize, vk::BufferUsageFlagBits::eTransferDst | usage,
                 VMA_MEMORY_USAGE_GPU_ONLY, outBuffer, outAllocation);

    // 4. Transfer Command
    copyBuffer(device, commandPool, queue, stagingBuffer, outBuffer, bufferSize);

    // 5. Cleanup
    vmaDestroyBuffer(allocator, stagingBuffer, stagingAlloc);
}


}