//
// Created by johnny on 2/25/26.
//

#pragma once
#include "common/VulkanInclude.hpp"

class VulkanUtils {
public:
    static void createBuffer(
        VmaAllocator allocator,
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        VmaMemoryUsage vmaUsage,
        vk::Buffer &buffer,
        VmaAllocation &allocation,
        VmaAllocationCreateFlags vmaFlags = 0) {
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

    static void copyBuffer(
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
};