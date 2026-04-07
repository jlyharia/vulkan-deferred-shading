#pragma once

#include "common/VulkanInclude.hpp"
#include <vector>

#include "../scene/Camera.hpp"
#include "common/Material.hpp"
#include "common/InstanceData.hpp"
#include "scene/DirLightView.hpp"
#include "scene/MeshInstance.hpp"
#include "scene/PointLight.hpp"


#include <memory>

#include "common/Config.hpp"

class SsaoBlurPass;
class SsaoPass;
struct Texture;
class Mesh;
class UserInterface;
class GraphicsPipeline;
class GBuffer;
class SwapChain;
class VulkanContext;
class ShadowMap;
struct GeometryPass;
struct LightingPass;
struct OverlayPass;
struct DirShadowPass;


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

    void drawFrame(const GraphicsPipeline &graphicsPipeline,
                   bool framebufferResized,
                   const Camera &camera,
                   const UserInterface &userInterface,
                   const std::vector<MeshInstance> &meshInstances,
                   const std::vector<PointLight> &pointLights,
                   const DirLightView &dirLight);

    void recreateSwapChain();

    [[nodiscard]] vk::DescriptorSetLayout getGlobalDescriptorSetLayout() const { return globalDescriptorSetLayout_; }
    [[nodiscard]] vk::DescriptorSetLayout getTextureDescriptorSetLayout() const { return textureLayout_; }
    [[nodiscard]] vk::DescriptorPool      getDescriptorPool()            const { return descriptorPool_; }

    [[nodiscard]] std::vector<vk::DescriptorSetLayout> getDescriptorSetLayouts() const {
        // The order here MUST match your set = 0, set = 1, set = 2 in GLSL
        return {globalDescriptorSetLayout_, textureLayout_, lightingInputsLayout_, ssaoBufferLayout_};
    }

    [[nodiscard]] vk::DescriptorSetLayout getSsaoBlurLayout() const { return ssaoBlurLayout_; }

    [[nodiscard]] GBuffer &getGBuffer() const { return *gbuffer_; }

    vk::DescriptorSet createTextureDescriptorSet(
        vk::ImageView imageView,
        vk::ImageView normalView,
        vk::ImageView metallicRoughnessView,
        vk::Sampler sampler);

    void setupDefaultMaterial(vk::ImageView whiteView,
                              vk::ImageView normalView,
                              vk::ImageView blackView,
                              vk::Sampler sampler);

    void setSphereMesh(std::shared_ptr<Mesh> mesh) { sphereMesh_ = std::move(mesh); }

private:
    void createCommandBuffers();
    void createSyncObjects();

    void recordCommandBuffer(vk::CommandBuffer cmd,
                             const GraphicsPipeline &graphicsPipeline,
                             uint32_t imageIndex,
                             const UserInterface &userInterface,
                             const std::vector<MeshInstance> &meshInstances,
                             uint32_t instanceCount,
                             const DirLightView &dirLight) const;
    void prepareFrameImages(vk::CommandBuffer cmd, uint32_t imageIndex) const;
    void finalizeFrameImages(vk::CommandBuffer cmd, uint32_t imageIndex) const;

    void createUniformBuffers();
    void updateUniformBuffer(uint32_t currentFrame, const Camera &camera,
                             const std::vector<PointLight> &pointLights,
                             const DirLightView &dirLight) const;
    void createInstanceBuffers();
    [[nodiscard]] uint32_t updateInstanceBuffer(uint32_t currentFrame,
                                                const std::vector<MeshInstance> &meshInstances) const;
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();
    void createLightingInputsDescSets();
    void updateLightingInputsDescSets();
    void createSsaoDescriptorSets();
    void createSsaoBlurDescriptorSet();
    void updateSsaoDescriptorSets();
    void updateSsaoBlurDescriptorSet();

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

    // Instance Resources
    std::vector<vk::Buffer> instanceBuffers_;
    std::vector<VmaAllocation> instanceAllocations_;
    std::vector<void *> instanceBuffersMapped_;

    std::shared_ptr<Mesh> sphereMesh_;

    // Descriptors (C++ style)
    vk::DescriptorPool descriptorPool_;
    std::vector<vk::DescriptorSet> descriptorSets_;
    vk::DescriptorSetLayout globalDescriptorSetLayout_;
    vk::DescriptorSetLayout textureLayout_;

    // Deferred shading resources
    std::unique_ptr<GBuffer> gbuffer_;
    vk::DescriptorSetLayout lightingInputsLayout_;
    std::vector<vk::DescriptorSet> lightingInputsDescSets_; // one per frame-in-flight
    // todo should reuse gbuffer?
    vk::Sampler nearestClampSampler_; // nearest-neighbor, clamp-to-edge — shared by gbuffer + SSAO reads

    vk::DescriptorSetLayout ssaoBufferLayout_;
    vk::DescriptorSet ssaoBufferDescriptorSet_;
    vk::DescriptorSetLayout ssaoBlurLayout_;
    vk::DescriptorSet ssaoBlurDescriptorSet_;
    vk::Sampler ssaoNoiseSampler_;
    // Render passes (initialized in initResources, after GBuffer is ready)
    std::unique_ptr<ShadowMap> shadowMap_;
    std::unique_ptr<DirShadowPass> dirShadowPass_;

    std::unique_ptr<GeometryPass> geometryPass_;
    std::unique_ptr<SsaoPass> ssaoPass_;
    std::unique_ptr<SsaoBlurPass> ssaoBlurPass_;
    std::unique_ptr<LightingPass> lightingPass_;
    std::unique_ptr<OverlayPass> overlayPass_;

    Material defaultMaterial;
};