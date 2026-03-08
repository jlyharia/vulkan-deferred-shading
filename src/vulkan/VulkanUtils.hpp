//
// Created by johnny on 2/25/26.
//

#pragma once
#include "common/VulkanInclude.hpp"

namespace vk_util {

// 1. Generic Buffer Creation (VMA)
void createBuffer(
    VmaAllocator allocator,
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    VmaMemoryUsage vmaUsage,
    vk::Buffer &buffer,
    VmaAllocation &allocation,
    VmaAllocationCreateFlags vmaFlags = 0);

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
    VmaAllocation &allocation);

// 3. Image Layout Transition (Essential for Textures)
void transitionImageLayout(
    vk::Device device,
    vk::CommandPool commandPool,
    vk::Queue graphicsQueue,
    vk::Image image,
    vk::Format format,
    vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout);

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
    vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor
    );

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