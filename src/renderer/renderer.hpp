#pragma once

#include "common/VulkanInclude.hpp"
#include <string>
#include <vector>

#include "../scene/Camera.hpp"
#include "scene/RenderObject.hpp"


#include <memory>

class Model;
class UserInterface;
// Forward declarations
class RenderPass;
class SwapChain;
class VulkanContext;

class Renderer {
public:
    Renderer(VulkanContext &context, SwapChain &swapChain, GLFWwindow *window);
    ~Renderer();

    // Disable copying
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;

    void initResources(vk::PipelineLayout pipelineLayout);
    void createDescriptorSetLayout();

    // Changed to vk::Pipeline for C++ style consistency
    void drawFrame(vk::Pipeline pipeline,
                   bool framebufferResized,
                   const Camera &camera,
                   UserInterface &userInterface,
                   const std::vector<RenderObject> &renderObjects);

    void recreateSwapChain();

    [[nodiscard]] vk::DescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout_; }

    [[nodiscard]] vk::CommandPool getCommandPool() const { return commandPool_; }

private:
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();

    // Updated to use vk:: types
    void recordCommandBuffer(vk::CommandBuffer cmd,
                             vk::Pipeline pipeline,
                             uint32_t imageIndex,
                             const UserInterface &userInterface,
                             const std::vector<RenderObject> &objects) const;
    void renderScene(vk::CommandBuffer cmd,
                     vk::Pipeline pipeline,
                     uint32_t imageIndex,
                     const std::vector<RenderObject> &objects) const;
    vk::RenderingAttachmentInfo getPrimaryColorAttachment(uint32_t imageIndex) const;
    vk::RenderingAttachmentInfo getPrimaryDepthAttachment() const;
    void prepareFrameImages(vk::CommandBuffer cmd, uint32_t imageIndex) const;
    void finalizeFrameImages(vk::CommandBuffer cmd, uint32_t imageIndex) const;

    // Your updated C++ style buffer helper
    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, VmaMemoryUsage vmaUsage, vk::Buffer &buffer,
                      VmaAllocation &allocation, VmaAllocationCreateFlags vmaFlags = 0,
                      VmaAllocationInfo *outAllocInfo = nullptr) const;

    void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) const;

    void createUniformBuffers();
    void updateUniformBuffer(uint32_t currentFrame, const Camera &camera) const;
    void createDescriptorPool();
    void createDescriptorSets();
    void transitionImageLayout(vk::CommandBuffer cmd, vk::Image image,
                               vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                               vk::ImageAspectFlags aspectMask) const;


    // --- Members ---
    VulkanContext &context_;
    SwapChain &swapChain_;
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

    // Uniform Resources
    std::vector<vk::Buffer> uniformBuffers_;
    std::vector<VmaAllocation> uniformBuffersAllocation_;
    std::vector<void *> uniformBuffersMapped_;

    // Descriptors (C++ style)
    vk::DescriptorPool descriptorPool_;
    std::vector<vk::DescriptorSet> descriptorSets_;
    vk::DescriptorSetLayout descriptorSetLayout_;

    // ModelSystem ms;
    std::unique_ptr<Model> model_;
};