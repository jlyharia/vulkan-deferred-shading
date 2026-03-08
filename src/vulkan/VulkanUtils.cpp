//
// Created by johnny on 3/3/26.
//
#include "VulkanUtils.hpp"

namespace vk_util {

void createBuffer(
    VmaAllocator allocator,
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    VmaMemoryUsage vmaUsage,
    vk::Buffer &buffer,
    VmaAllocation &allocation,
    VmaAllocationCreateFlags vmaFlags) {
    VkBufferCreateInfo bufferInfo = vk::BufferCreateInfo()
                                    .setSize(size)
                                    .setUsage(usage)
                                    .setSharingMode(vk::SharingMode::eExclusive);

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = vmaUsage;
    allocInfo.flags = vmaFlags;

    VkBuffer rawBuffer;
    if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &rawBuffer, &allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer via VMA!");
    }
    buffer = rawBuffer;
}

void copyBuffer(
    vk::Device device,
    vk::CommandPool commandPool,
    vk::Queue queue,
    vk::Buffer srcBuffer,
    vk::Buffer dstBuffer,
    vk::DeviceSize size) {

    vk::CommandBufferAllocateInfo allocInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1);
    auto cmd = device.allocateCommandBuffers(allocInfo)[0];

    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    cmd.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
    cmd.end();

    // 1. Create the Fence
    vk::FenceCreateInfo fenceInfo{};
    auto fence = device.createFence(fenceInfo);

    // 2. Submit with the Fence
    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBuffers(cmd);
    queue.submit(submitInfo, fence); // The fence will be "signaled" when done

    // 3. Wait for the Fence specifically
    // We wait indefinitely (UINT64_MAX) until the copy is finished
    auto result = device.waitForFences(fence, VK_TRUE, std::numeric_limits<uint64_t>::max());

    // 4. Cleanup
    device.destroyFence(fence);
    device.freeCommandBuffers(commandPool, cmd);
}

void createImage(VmaAllocator allocator,
                 uint32_t width,
                 uint32_t height,
                 vk::Format format,
                 vk::ImageTiling tiling,
                 vk::ImageUsageFlags usage,
                 VmaMemoryUsage vmaUsage,
                 vk::Image &image,
                 VmaAllocation &allocation) {

    vk::ImageCreateInfo imageInfo{};
    imageInfo.setImageType(vk::ImageType::e2D)
             .setExtent({width, height, 1})
             .setMipLevels(1)
             .setArrayLayers(1)
             .setFormat(format)
             .setTiling(tiling)
             .setInitialLayout(vk::ImageLayout::eUndefined)
             .setUsage(usage)
             .setSamples(vk::SampleCountFlagBits::e1)
             .setSharingMode(vk::SharingMode::eExclusive);

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = vmaUsage;

    VkImage rawImage;
    auto result = vmaCreateImage(allocator, reinterpret_cast<const VkImageCreateInfo *>(&imageInfo),
                                 &allocInfo, &rawImage, &allocation, nullptr);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create VMA image");
    image = rawImage;
}

void transitionImageLayout(vk::Device device,
                           vk::CommandPool commandPool,
                           vk::Queue graphicsQueue,
                           vk::Image image,
                           vk::Format format,
                           vk::ImageLayout oldLayout,
                           vk::ImageLayout newLayout) {

    auto cmd = beginSingleTimeCommands(device, commandPool);

    vk::ImageMemoryBarrier barrier{};
    barrier.setOldLayout(oldLayout)
           .setNewLayout(newLayout)
           .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setImage(image)
           .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    // Simple state machine for common transitions
    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.setSrcAccessMask(vk::AccessFlagBits::eNone);
        barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout ==
               vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
        barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    cmd.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, barrier);
    endSingleTimeCommands(device, commandPool, graphicsQueue, cmd);
}

vk::ImageView createImageView(
    const vk::Device device,
    const vk::Image image,
    const vk::Format format,
    const vk::ImageAspectFlags aspectFlags) {
    // C++20/Vulkan-HPP Chained Setter Style
    vk::ImageViewCreateInfo viewInfo = vk::ImageViewCreateInfo()
                                       .setImage(image)
                                       .setViewType(vk::ImageViewType::e2D)
                                       .setFormat(format)
                                       .setSubresourceRange(vk::ImageSubresourceRange()
                                                            .setAspectMask(aspectFlags)
                                                            .setBaseMipLevel(0)
                                                            .setLevelCount(1)
                                                            .setBaseArrayLayer(0)
                                                            .setLayerCount(1));

    return device.createImageView(viewInfo);
}

void copyBufferToImage(
    const vk::Device device,
    const vk::CommandPool commandPool,
    const vk::Queue graphicsQueue,
    const vk::Buffer buffer,
    const vk::Image image,
    const uint32_t width,
    const uint32_t height) {
    auto cmd = beginSingleTimeCommands(device, commandPool);

    // C++20 Designated Initializers / Chained setters
    vk::BufferImageCopy region = vk::BufferImageCopy()
                                 .setBufferOffset(0)
                                 .setBufferRowLength(0) // Tightly packed
                                 .setBufferImageHeight(0) // Tightly packed
                                 .setImageSubresource(vk::ImageSubresourceLayers()
                                                      .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                                      .setMipLevel(0)
                                                      .setBaseArrayLayer(0)
                                                      .setLayerCount(1))
                                 .setImageOffset({0, 0, 0})
                                 .setImageExtent({width, height, 1});

    cmd.copyBufferToImage(
        buffer,
        image,
        vk::ImageLayout::eTransferDstOptimal,
        region
        );

    endSingleTimeCommands(device, commandPool, graphicsQueue, cmd);
}

vk::CommandBuffer beginSingleTimeCommands(vk::Device device,
                                          vk::CommandPool commandPool) {
    // 1. Setup allocation info for a one-off primary command buffer
    vk::CommandBufferAllocateInfo allocInfo = vk::CommandBufferAllocateInfo()
                                              .setCommandPool(commandPool)
                                              .setLevel(vk::CommandBufferLevel::ePrimary)
                                              .setCommandBufferCount(1);

    // 2. Allocate and begin recording immediately
    auto commandBuffer = device.allocateCommandBuffers(allocInfo)[0];

    vk::CommandBufferBeginInfo beginInfo = vk::CommandBufferBeginInfo()
        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void endSingleTimeCommands(vk::Device device,
                           vk::CommandPool commandPool,
                           vk::Queue queue,
                           vk::CommandBuffer commandBuffer) {
    // 1. Stop recording
    commandBuffer.end();

    // 2. Submit the command buffer to the queue
    vk::SubmitInfo submitInfo = vk::SubmitInfo()
        .setCommandBuffers(commandBuffer);

    // 3. Create a fence so we can wait for the GPU to finish
    // A fence is better than device.waitIdle() because it only blocks until THIS work is done
    vk::Fence fence = device.createFence(vk::FenceCreateInfo());

    queue.submit(submitInfo, fence);

    // 4. Wait for the fence (infinite timeout)
    auto result = device.waitForFences(fence, VK_TRUE, std::numeric_limits<uint64_t>::max());

    // 5. Cleanup temporary resources
    device.destroyFence(fence);
    device.freeCommandBuffers(commandPool, commandBuffer);
}

}