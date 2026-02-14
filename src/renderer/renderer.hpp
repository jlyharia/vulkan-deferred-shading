#pragma once

#include <string>
#include <vector>
#include <GLFW/glfw3.h>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Camera.hpp"
#include "system/ModelSystem.hpp"

// Forward declarations
class RenderPass;
class SwapChain;
class VulkanContext;

class Renderer {
public:
    Renderer(VulkanContext &context,
             SwapChain &swapChain,
             RenderPass &renderPass,
             GLFWwindow *window);
    ~Renderer();

    // Disable copying
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;

    void initResources(vk::PipelineLayout pipelineLayout, std::string modelPath);
    void createDescriptorSetLayout();

    // Changed to vk::Pipeline for C++ style consistency
    void drawFrame(vk::Pipeline pipeline, bool framebufferResized, const Camera &camera);

    void recreateSwapChain();

    [[nodiscard]] vk::DescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout_; }

private:
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();

    // Updated to use vk:: types
    void recordCommandBuffer(vk::CommandBuffer commandBuffer, vk::Pipeline pipeline, uint32_t imageIndex) const;

    void createVertexBuffer();
    void createIndexBuffer();

    // Your updated C++ style buffer helper
    void createBuffer(vk::DeviceSize size,
                      vk::BufferUsageFlags usage,
                      VmaMemoryUsage vmaUsage,
                      vk::Buffer &buffer,
                      VmaAllocation &allocation,
                      VmaAllocationCreateFlags vmaFlags = 0,
                      VmaAllocationInfo *outAllocInfo = nullptr) const;

    void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) const;

    void createAllocator();
    void createUniformBuffers();
    void updateUniformBuffer(uint32_t currentFrame, const Camera &camera) const;
    void createDescriptorPool();
    void createDescriptorSets();

    // --- Members ---
    VulkanContext &context_;
    SwapChain &swapChain_;
    RenderPass &renderPass_;
    GLFWwindow *window_;

    // Core Vulkan Handles (C++ style)
    vk::PipelineLayout activePipelineLayout_;
    vk::CommandPool commandPool_;
    std::vector<vk::CommandBuffer> commandBuffers_;

    // Synchronization (C++ style)
    std::vector<vk::Semaphore> imageAvailableSemaphores_;
    std::vector<vk::Semaphore> renderFinishedSemaphores_;
    std::vector<vk::Fence> inFlightFences_;
    std::vector<vk::Fence> imagesInFlight;

    uint32_t currentFrame = 0;

    // Memory Resources (VMA + vk::Buffer)
    VmaAllocator vmaAllocator = nullptr;

    vk::Buffer vertexBuffer_;
    VmaAllocation vertexBufferAllocation_ = nullptr;

    vk::Buffer indexBuffer_;
    VmaAllocation indexBufferAllocation_ = nullptr;

    // Uniform Resources
    std::vector<vk::Buffer> uniformBuffers_;
    std::vector<VmaAllocation> uniformBuffersAllocation_;
    std::vector<void *> uniformBuffersMapped_;

    // Descriptors (C++ style)
    vk::DescriptorPool descriptorPool_;
    std::vector<vk::DescriptorSet> descriptorSets_;
    vk::DescriptorSetLayout descriptorSetLayout_;

    ModelSystem ms;
};