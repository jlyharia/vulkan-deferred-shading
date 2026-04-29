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
    VmaAllocationCreateFlags vmaFlags,
    VmaAllocationInfo *outAllocInfo) {
    VkBufferCreateInfo bufferInfo = vk::BufferCreateInfo()
                                    .setSize(size)
                                    .setUsage(usage)
                                    .setSharingMode(vk::SharingMode::eExclusive);

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = vmaUsage;
    allocInfo.flags = vmaFlags;

    VkBuffer rawBuffer;
    if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &rawBuffer, &allocation, outAllocInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer via VMA!");
    }
    buffer = rawBuffer;
}

void copyBuffer(
    const vk::Device device,
    const vk::CommandPool commandPool,
    const vk::Queue queue,
    const vk::Buffer srcBuffer,
    const vk::Buffer dstBuffer,
    const vk::DeviceSize size) {

    vk::CommandBufferAllocateInfo allocInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1);
    auto cmd = device.allocateCommandBuffers(allocInfo)[0];

    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    cmd.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
    cmd.end();

    // 1. Create the Fence
    vk::FenceCreateInfo fenceInfo{};
    const auto fence = device.createFence(fenceInfo);

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

void createImage(const VmaAllocator allocator,
                 uint32_t width,
                 uint32_t height,
                 const vk::Format format,
                 const vk::ImageTiling tiling,
                 const vk::ImageUsageFlags usage,
                 const VmaMemoryUsage vmaUsage,
                 vk::Image &image,
                 VmaAllocation &allocation,
                 const uint32_t mipLevels) {

    vk::ImageCreateInfo imageInfo{};
    imageInfo.setImageType(vk::ImageType::e2D)
             .setExtent({width, height, 1})
             .setMipLevels(mipLevels)
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

void transitionImageLayout(const vk::Device device,
                           const vk::CommandPool commandPool,
                           const vk::Queue graphicsQueue,
                           const vk::Image image,
                           const vk::Format format,
                           const vk::ImageLayout oldLayout,
                           const vk::ImageLayout newLayout,
                           const uint32_t mipLevels) {

    auto cmd = beginSingleTimeCommands(device, commandPool);

    vk::ImageMemoryBarrier barrier{};
    barrier.setOldLayout(oldLayout)
           .setNewLayout(newLayout)
           .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setImage(image)
           .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1});

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
    const vk::ImageAspectFlags aspectFlags,
    const uint32_t mipLevels) {
    // C++20/Vulkan-HPP Chained Setter Style
    vk::ImageViewCreateInfo viewInfo = vk::ImageViewCreateInfo()
                                       .setImage(image)
                                       .setViewType(vk::ImageViewType::e2D)
                                       .setFormat(format)
                                       .setSubresourceRange(vk::ImageSubresourceRange()
                                                            .setAspectMask(aspectFlags)
                                                            .setBaseMipLevel(0)
                                                            .setLevelCount(mipLevels)
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

AttachmentImage AttachmentImage::create(
    VmaAllocator         allocator,
    vk::Device           device,
    uint32_t             width,
    uint32_t             height,
    vk::Format           format,
    vk::ImageUsageFlags  usage,
    vk::ImageAspectFlags aspectFlags) {

    AttachmentImage result;
    createImage(allocator, width, height, format, vk::ImageTiling::eOptimal,
                usage, VMA_MEMORY_USAGE_GPU_ONLY, result.image, result.allocation);
    result.view = createImageView(device, result.image, format, aspectFlags);
    return result;
}

void AttachmentImage::cleanup(vk::Device device, VmaAllocator allocator) noexcept {
    if (view)  { device.destroyImageView(view);                        view  = nullptr; }
    if (image) { vmaDestroyImage(allocator, image, allocation); image = nullptr; allocation = nullptr; }
}

vk::ImageAspectFlags imageAspect(vk::Format format) noexcept {
    switch (format) {
        case vk::Format::eD16Unorm:
        case vk::Format::eD32Sfloat:
        case vk::Format::eD16UnormS8Uint:
        case vk::Format::eD24UnormS8Uint:
        case vk::Format::eD32SfloatS8Uint:
            return vk::ImageAspectFlagBits::eDepth;
        default:
            return vk::ImageAspectFlagBits::eColor;
    }
}

vk::ImageMemoryBarrier2 colorAttachmentToShaderRead(vk::Image image) noexcept {
    return vk::ImageMemoryBarrier2()
           .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
           .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
           .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
           .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
           .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
           .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
           .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setImage(image)
           .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
}

vk::ImageMemoryBarrier2 depthToShaderRead(vk::Image image) noexcept {
    return vk::ImageMemoryBarrier2()
           .setSrcStageMask(vk::PipelineStageFlagBits2::eLateFragmentTests)
           .setSrcAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
           .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
           .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
           .setOldLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
           .setNewLayout(vk::ImageLayout::eDepthReadOnlyOptimal)
           .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setImage(image)
           .setSubresourceRange({vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1});
}

vk::ImageMemoryBarrier2 undefinedToColorAttachment(vk::Image image) noexcept {
    return vk::ImageMemoryBarrier2()
           .setOldLayout(vk::ImageLayout::eUndefined)
           .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
           .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
           .setSrcAccessMask(vk::AccessFlagBits2::eNone)
           .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
           .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
           .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setImage(image)
           .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
}

vk::ImageMemoryBarrier2 colorAttachmentToPresent(vk::Image image) noexcept {
    return vk::ImageMemoryBarrier2()
           .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
           .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
           .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
           .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
           .setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe)
           .setDstAccessMask(vk::AccessFlagBits2::eNone)
           .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setImage(image)
           .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
}

vk::ImageMemoryBarrier2 undefinedToDepthAttachment(vk::Image image) noexcept {
    return vk::ImageMemoryBarrier2()
           .setOldLayout(vk::ImageLayout::eUndefined)
           .setNewLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
           .setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
           .setSrcAccessMask(vk::AccessFlagBits2::eNone)
           .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                            vk::PipelineStageFlagBits2::eLateFragmentTests)
           .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentRead |
                             vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
           .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setImage(image)
           .setSubresourceRange({vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1});
}

vk::ImageMemoryBarrier2 depthAttachmentToShaderRead(vk::Image image) noexcept {
    return vk::ImageMemoryBarrier2()
           .setOldLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
           .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
           .setSrcStageMask(vk::PipelineStageFlagBits2::eLateFragmentTests)
           .setSrcAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
           .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
           .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
           .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setImage(image)
           .setSubresourceRange({vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1});
}

vk::WriteDescriptorSet imageSamplerWrite(
    vk::DescriptorSet              set,
    uint32_t                       binding,
    const vk::DescriptorImageInfo &imageInfo) noexcept {
    return vk::WriteDescriptorSet()
           .setDstSet(set)
           .setDstBinding(binding)
           .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
           .setDescriptorCount(1)
           .setPImageInfo(&imageInfo);
}

void uploadToDeviceImage(
    VmaAllocator    allocator,
    vk::Device      device,
    vk::CommandPool commandPool,
    vk::Queue       queue,
    const void     *data,
    vk::DeviceSize  dataSize,
    uint32_t        width,
    uint32_t        height,
    vk::Format      format,
    vk::Image      &outImage,
    VmaAllocation  &outAlloc) {
    // 1. Staging buffer
    vk::Buffer stagingBuffer;
    VmaAllocation stagingAlloc;
    createBuffer(allocator, dataSize, vk::BufferUsageFlagBits::eTransferSrc,
                 VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingAlloc);

    void *mapped;
    vmaMapMemory(allocator, stagingAlloc, &mapped);
    memcpy(mapped, data, static_cast<size_t>(dataSize));
    vmaUnmapMemory(allocator, stagingAlloc);

    // 2. GPU image
    createImage(allocator, width, height, format, vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                VMA_MEMORY_USAGE_GPU_ONLY, outImage, outAlloc);

    // 3. Upload: undefined → transfer-dst → shader-read
    transitionImageLayout(device, commandPool, queue,
                          outImage, format,
                          vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(device, commandPool, queue, stagingBuffer, outImage, width, height);
    transitionImageLayout(device, commandPool, queue,
                          outImage, format,
                          vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    vmaDestroyBuffer(allocator, stagingBuffer, stagingAlloc);
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

// =============================================================================
// Debug labels
// =============================================================================
static PFN_vkCmdBeginDebugUtilsLabelEXT pfnBeginLabel = nullptr;
static PFN_vkCmdEndDebugUtilsLabelEXT   pfnEndLabel   = nullptr;

void initDebugLabels(VkInstance instance) noexcept {
    pfnBeginLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
    pfnEndLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));
}

void cmdBeginLabel(vk::CommandBuffer cmd, std::string_view name) noexcept {
    if (!pfnBeginLabel) return;
    VkDebugUtilsLabelEXT info{};
    info.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    info.pLabelName = name.data();
    pfnBeginLabel(cmd, &info);
}

void cmdEndLabel(vk::CommandBuffer cmd) noexcept {
    if (!pfnEndLabel) return;
    pfnEndLabel(cmd);
}

}