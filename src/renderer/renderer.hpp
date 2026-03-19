#pragma once

#include "common/VulkanInclude.hpp"
#include <vector>

#include "../scene/Camera.hpp"
#include "common/Material.hpp"
#include "scene/MeshInstance.hpp"
#include "scene/PointLight.hpp"


#include <memory>


struct Texture;
class Mesh;
class UserInterface;
class GraphicsPipeline;
// Forward declarations
class RenderPass;
class SwapChain;
class VulkanContext;


class Renderer {
public:
    Renderer(VulkanContext &context,
             SwapChain &swapChain,
             GLFWwindow *window);
    ~Renderer();

    // Disable copying
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;

    void initResources();
    void createDescriptorSetLayout();

    void drawFrame(const GraphicsPipeline &graphicsPipeline,
                   bool framebufferResized,
                   const Camera &camera,
                   const UserInterface &userInterface,
                   const std::vector<MeshInstance> &meshInstances,
                   const std::vector<PointLight> &pointLights);

    void recreateSwapChain() const;

    [[nodiscard]] vk::DescriptorSetLayout getGlobalDescriptorSetLayout() const { return globalDescriptorSetLayout_; }
    [[nodiscard]] vk::DescriptorSetLayout getTextureDescriptorSetLayout() const { return textureLayout_; }

    [[nodiscard]] std::vector<vk::DescriptorSetLayout> getDescriptorSetLayouts() const {
        // The order here MUST match your set = 0, set = 1 in GLSL
        return {globalDescriptorSetLayout_, textureLayout_};
    }

    vk::DescriptorSet createTextureDescriptorSet(
        vk::ImageView imageView,
        vk::ImageView normalView,
        vk::ImageView metallicRoughnessView,
        vk::Sampler sampler);

    void setupDefaultMaterial(vk::ImageView whiteView,
                              vk::ImageView normalView,
                              vk::ImageView blackView,
                              vk::Sampler sampler);
private:
    void createCommandBuffers();
    void createSyncObjects();

    void recordCommandBuffer(vk::CommandBuffer cmd,
                             const GraphicsPipeline &graphicsPipeline,
                             uint32_t imageIndex,
                             const UserInterface &userInterface,
                             const std::vector<MeshInstance> &meshInstances) const;
    void renderScene(vk::CommandBuffer cmd,
                     const GraphicsPipeline &graphicsPipeline,
                     uint32_t imageIndex,
                     const std::vector<MeshInstance> &meshInstances) const;
    [[nodiscard]] vk::RenderingAttachmentInfo getPrimaryColorAttachment(uint32_t imageIndex) const;
    [[nodiscard]] vk::RenderingAttachmentInfo getPrimaryDepthAttachment() const;
    void prepareFrameImages(vk::CommandBuffer cmd, uint32_t imageIndex) const;
    void finalizeFrameImages(vk::CommandBuffer cmd, uint32_t imageIndex) const;

    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, VmaMemoryUsage vmaUsage, vk::Buffer &buffer,
                      VmaAllocation &allocation, VmaAllocationCreateFlags vmaFlags = 0,
                      VmaAllocationInfo *outAllocInfo = nullptr) const;

    void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) const;

    void createUniformBuffers();
    void updateUniformBuffer(uint32_t currentFrame, const Camera &camera,
                             const std::vector<PointLight> &pointLights) const;
    void createDescriptorPool();
    void createDescriptorSets();

    void transitionImageLayout(vk::CommandBuffer cmd,
                               vk::Image image,
                               vk::ImageLayout oldLayout,
                               vk::ImageLayout newLayout,
                               vk::ImageAspectFlags aspectMask) const;


    // --- Members ---
    VulkanContext &context_;
    SwapChain &swapChain_;
    GLFWwindow *window_;

    // Core Vulkan Handles (C++ style)
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
    vk::DescriptorSetLayout globalDescriptorSetLayout_;
    vk::DescriptorSetLayout textureLayout_;

    Material defaultMaterial;
};
