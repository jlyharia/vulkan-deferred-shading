//
// Created by johnny on 12/29/25.
//

#include "Renderer.hpp"
#include "GpuTimestamps.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cassert>
#include <chrono>
#include <cstring>

#include "../common/Uniform.hpp"
#include "../common/InstanceData.hpp"
#include "UserInterface.hpp"
#include "../common/Vertex.hpp"
#include "common/Config.hpp"
#include "scene/Mesh.hpp"
#include "passes/SsaoPass.hpp"
#include "passes/SsaoBlurPass.hpp"
#include "passes/graph/RenderGraph.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/SwapChain.hpp"
#include "vulkan/GBuffer.hpp"
#include "vulkan/VulkanUtils.hpp"
#include "renderer/passes/DirShadowPass.hpp"
#include "renderer/passes/GeometryPass.hpp"
#include "renderer/passes/LightingPass.hpp"
#include "renderer/passes/OverlayPass.hpp"
#include "vulkan/ShadowMap.hpp"


// The C++ Bindings Header

Renderer::Renderer(VulkanContext &context, SwapChain &swapChain, GLFWwindow *window_)
    : context_(context), swapChain_(swapChain), window_(window_) {

    // 2. Initialize Command Infrastructure
    createCommandBuffers();

    // 3. Setup Synchronization (Fences/Semaphores)
    createSyncObjects();
}

/**
* In Vulkan, the specific order of destruction between a Semaphore and a Command Pool does not technically matter, as
long as they are both destroyed after the GPU has finished using them.
*
* However, there is a "Logical Best Practice" that most engine developers follow to keep code clean and mirror the
creation order.
*
* Destruction Order Checklist

Always follow the "Last In, First Out" (LIFO) rule relative to the Logical Device.

   1. Wait for GPU to finish (vkDeviceWaitIdle).

   2. Destroy Resources (Buffers, ImageViews, Pipelines).

   3. Destroy Sync Objects (Fences, Semaphores).

   4. Destroy Pools (Command Pool, Descriptor Pool).

   5. Destroy Device (The Logical Device handle).
 */
Renderer::~Renderer() {
    // 1. Ensure GPU is idle before we start deleting things
    std::cerr << "[Destructor] Renderer starting..." << std::endl;
    vkDeviceWaitIdle(context_.getDevice());

    gpuTimestamps_.reset(); // query pools must go before device
    // Destroy pass resources before pool (sets are freed when pool is destroyed)
    dirShadowPass_.reset();
    shadowMap_.reset();
    ssaoBlurPass_.reset();
    ssaoPass_.reset();
    gbuffer_.reset();
    if (ssaoNoiseSampler_) {
        context_.getDevice().destroySampler(ssaoNoiseSampler_);
        ssaoNoiseSampler_ = nullptr;
    }
    if (ssaoBlurLayout_) {
        context_.getDevice().destroyDescriptorSetLayout(ssaoBlurLayout_);
        ssaoBlurLayout_ = nullptr;
    }
    if (ssaoBufferLayout_) {
        context_.getDevice().destroyDescriptorSetLayout(ssaoBufferLayout_);
        ssaoBufferLayout_ = nullptr;
    }
    if (nearestClampSampler_) {
        context_.getDevice().destroySampler(nearestClampSampler_);
        nearestClampSampler_ = nullptr;
    }
    if (lightingInputsLayout_) {
        vkDestroyDescriptorSetLayout(context_.getDevice(), lightingInputsLayout_, nullptr);
        lightingInputsLayout_ = nullptr;
    }

    vkDestroyDescriptorPool(context_.getDevice(), descriptorPool_, nullptr);

    vkDestroyDescriptorSetLayout(context_.getDevice(), globalDescriptorSetLayout_, nullptr);

    if (textureLayout_) {
        vkDestroyDescriptorSetLayout(context_.getDevice(), textureLayout_, nullptr);
    }

    for (size_t i = 0; i < engineConfig::MAX_FRAMES_IN_FLIGHT; i++) {
        // VMA automatically handles the Unmapping if you used
        // VMA_ALLOCATION_CREATE_MAPPED_BIT.
        if (uniformBuffers_[i] != VK_NULL_HANDLE) {
            vmaDestroyBuffer(context_.getVmaAllocator(), uniformBuffers_[i], uniformBuffersAllocation_[i]);

            // Safety: Clear the handles
            uniformBuffers_[i] = VK_NULL_HANDLE;
            uniformBuffersAllocation_[i] = nullptr;
            uniformBuffersMapped_[i] = nullptr;
        }

        if (instanceBuffers_[i] != VK_NULL_HANDLE) {
            vmaDestroyBuffer(context_.getVmaAllocator(), instanceBuffers_[i], instanceAllocations_[i]);

            instanceBuffers_[i] = VK_NULL_HANDLE;
            instanceAllocations_[i] = nullptr;
            instanceBuffersMapped_[i] = nullptr;
        }
    }

    // 2. Destroy Fences (Per Frame Slot)
    for (const auto &fence : inFlightFences_) {
        vkDestroyFence(context_.getDevice(), fence, nullptr);
    }

    // 3. Destroy Semaphores (Per Swapchain Image)
    for (const auto &semaphore : imageAvailableSemaphores_) {
        vkDestroySemaphore(context_.getDevice(), semaphore, nullptr);
    }

    for (const auto &semaphore : renderFinishedSemaphores_) {
        vkDestroySemaphore(context_.getDevice(), semaphore, nullptr);
    }

}

void Renderer::initResources() {
    createDescriptorSetLayout();
    createUniformBuffers();
    createInstanceBuffers();
    createDescriptorPool();
    createDescriptorSets();

    // Shadow map: fixed resolution, independent of swapchain
    shadowMap_ = std::make_unique<ShadowMap>(context_, 2048, 2048);

    // G-buffer: sized to match swapchain extent
    auto extent = swapChain_.getExtent();
    gbuffer_ = std::make_unique<GBuffer>(context_, extent.width, extent.height);
    createLightingInputsDescSets(); // includes shadow map at binding 4

    // Render passes (GBuffer must exist before GeometryPass is constructed)
    dirShadowPass_ = std::make_unique<DirShadowPass>(*shadowMap_);
    geometryPass_ = std::make_unique<GeometryPass>(swapChain_, *gbuffer_);
    ssaoPass_ = std::make_unique<SsaoPass>(swapChain_, context_);
    createSsaoDescriptorSets();
    ssaoBlurPass_ = std::make_unique<SsaoBlurPass>(swapChain_, context_);
    createSsaoBlurDescriptorSet();
    lightingPass_ = std::make_unique<LightingPass>(swapChain_);
    overlayPass_ = std::make_unique<OverlayPass>(swapChain_);
    renderGraph_ = std::make_unique<RenderGraph>();
    rebuildRenderGraph();
    gpuTimestamps_ = std::make_unique<GpuTimestamps>(
        context_.getDevice(), context_.getPhysicalDevice());
}


std::vector<std::string> Renderer::buildPassNameList() const {
    auto names = renderGraph_->passNames();
    names.push_back("Overlay");
    return names;
}

const std::vector<GpuTimestamps::Entry> &Renderer::gpuTimings() const {
    static const std::vector<GpuTimestamps::Entry> empty{};
    return gpuTimestamps_ ? gpuTimestamps_->results() : empty;
}

// Texture names used in both registerTexture() and per-pass read/write declarations.
// Centralised here so a typo causes a compile error rather than a silent graph mis-wire.
namespace texName {
    constexpr std::string_view shadowMap      = "shadowMap";
    constexpr std::string_view albedoMetallic = "albedoMetallic";
    constexpr std::string_view normalRoughness= "normalRoughness";
    constexpr std::string_view ssaoBuffer     = "ssaoBuffer";
    constexpr std::string_view ssaoBlur       = "ssaoBlur";
    constexpr std::string_view gbufferDepth   = "gbufferDepth";
}

void Renderer::rebuildRenderGraph() {
    renderGraph_->reset();

    renderGraph_->registerTexture({
        .name          = std::string(texName::shadowMap),
        .image         = shadowMap_->getDepthImage(),
        .view          = shadowMap_->getDepthView(),
        .format        = ShadowMap::DEPTH_FORMAT,
        .initialLayout = vk::ImageLayout::eUndefined,
    });
    renderGraph_->registerTexture({
        .name          = std::string(texName::albedoMetallic),
        .image         = gbuffer_->getAlbedoMetallicImage(),
        .view          = gbuffer_->getAlbedoMetallicView(),
        .format        = GBuffer::ALBEDO_METALLIC_FORMAT,
        .initialLayout = vk::ImageLayout::eUndefined,
    });
    renderGraph_->registerTexture({
        .name          = std::string(texName::normalRoughness),
        .image         = gbuffer_->getNormalRoughnessImage(),
        .view          = gbuffer_->getNormalRoughnessView(),
        .format        = GBuffer::NORMAL_ROUGHNESS_FORMAT,
        .initialLayout = vk::ImageLayout::eUndefined,
    });
    renderGraph_->registerTexture({
        .name          = std::string(texName::ssaoBuffer),
        .image         = ssaoPass_->getSsaoBufferImage(),
        .view          = ssaoPass_->getSsaoKernelBufferImageView(),
        .format        = SsaoPass::SSAO_BUFFER_FORMAT,
        .initialLayout = vk::ImageLayout::eUndefined,
    });
    renderGraph_->registerTexture({
        .name          = std::string(texName::ssaoBlur),
        .image         = ssaoBlurPass_->getBlurredImage(),
        .view          = ssaoBlurPass_->getBlurredImageView(),
        .format        = SsaoBlurPass::SSAO_BLUR_BUFFER_FORMAT,
        .initialLayout = vk::ImageLayout::eUndefined,
    });
    // prepareFrameImages transitions depth Undefined→DepthStencilAttachment before graph runs
    renderGraph_->registerTexture({
        .name          = std::string(texName::gbufferDepth),
        .image         = swapChain_.getDepthImage(),
        .view          = swapChain_.getDepthImageView(),
        .format        = ShadowMap::DEPTH_FORMAT,
        .initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    });

    using Stage  = vk::PipelineStageFlagBits2;
    using Access = vk::AccessFlagBits2;
    using Layout = vk::ImageLayout;

    namespace tn = texName;

    renderGraph_->addPass({
        .name          = "DirShadow",
        .readTextures  = {},
        .writeTextures = {{
            .name   = std::string(tn::shadowMap),
            .layout = Layout::eDepthStencilAttachmentOptimal,
            .stage  = Stage::eEarlyFragmentTests | Stage::eLateFragmentTests,
            .access = Access::eDepthStencilAttachmentWrite,
        }},
        .execute = [this](vk::CommandBuffer cmd) {
            dirShadowPass_->execute(cmd, *graphicsPipeline_, *meshInstances_, *dirLight_);
        },
    });

    renderGraph_->addPass({
        .name          = "Geometry",
        .readTextures  = {},
        .writeTextures = {
            { std::string(tn::albedoMetallic),  Layout::eColorAttachmentOptimal,        Stage::eColorAttachmentOutput,                          Access::eColorAttachmentWrite },
            { std::string(tn::normalRoughness), Layout::eColorAttachmentOptimal,        Stage::eColorAttachmentOutput,                          Access::eColorAttachmentWrite },
            { std::string(tn::gbufferDepth),    Layout::eDepthStencilAttachmentOptimal, Stage::eEarlyFragmentTests | Stage::eLateFragmentTests, Access::eDepthStencilAttachmentWrite },
        },
        .execute = [this](vk::CommandBuffer cmd) {
            geometryPass_->execute(cmd, *graphicsPipeline_, descriptorSets_[currentFrame],
                                   *meshInstances_, defaultMaterial, sphereMesh_);
        },
    });

    renderGraph_->addPass({
        .name = "Ssao",
        .readTextures = {
            { std::string(tn::albedoMetallic),  Layout::eShaderReadOnlyOptimal, Stage::eFragmentShader, Access::eShaderSampledRead },
            { std::string(tn::normalRoughness), Layout::eShaderReadOnlyOptimal, Stage::eFragmentShader, Access::eShaderSampledRead },
            { std::string(tn::gbufferDepth),    Layout::eDepthReadOnlyOptimal,  Stage::eFragmentShader, Access::eShaderSampledRead },
        },
        .writeTextures = {{
            .name   = std::string(tn::ssaoBuffer),
            .layout = Layout::eColorAttachmentOptimal,
            .stage  = Stage::eColorAttachmentOutput,
            .access = Access::eColorAttachmentWrite,
        }},
        .execute = [this](vk::CommandBuffer cmd) {
            ssaoPass_->execute(cmd, *graphicsPipeline_,
                               descriptorSets_[currentFrame], lightingInputsDescSets_[currentFrame],
                               ssaoBufferDescriptorSet_);
        },
    });

    renderGraph_->addPass({
        .name = "SsaoBlur",
        .readTextures = {{
            .name   = std::string(tn::ssaoBuffer),
            .layout = Layout::eShaderReadOnlyOptimal,
            .stage  = Stage::eFragmentShader,
            .access = Access::eShaderSampledRead,
        }},
        .writeTextures = {{
            .name   = std::string(tn::ssaoBlur),
            .layout = Layout::eColorAttachmentOptimal,
            .stage  = Stage::eColorAttachmentOutput,
            .access = Access::eColorAttachmentWrite,
        }},
        .execute = [this](vk::CommandBuffer cmd) {
            ssaoBlurPass_->execute(cmd, *graphicsPipeline_, ssaoBlurDescriptorSet_);
        },
    });

    renderGraph_->addPass({
        .name = "Lighting",
        .readTextures = {
            { std::string(tn::albedoMetallic),  Layout::eShaderReadOnlyOptimal, Stage::eFragmentShader, Access::eShaderSampledRead },
            { std::string(tn::normalRoughness), Layout::eShaderReadOnlyOptimal, Stage::eFragmentShader, Access::eShaderSampledRead },
            { std::string(tn::ssaoBlur),        Layout::eShaderReadOnlyOptimal, Stage::eFragmentShader, Access::eShaderSampledRead },
            { std::string(tn::shadowMap),       Layout::eShaderReadOnlyOptimal, Stage::eFragmentShader, Access::eShaderSampledRead },
        },
        .writeTextures = {},
        .execute = [this](vk::CommandBuffer cmd) {
            lightingPass_->execute(cmd, *graphicsPipeline_, currentImageIndex_,
                                   descriptorSets_[currentFrame], lightingInputsDescSets_[currentFrame]);
        },
    });

    renderGraph_->compile();
}


void Renderer::createCommandBuffers() {
    commandBuffers_.resize(engineConfig::MAX_FRAMES_IN_FLIGHT);

    auto allocInfo = vk::CommandBufferAllocateInfo()
                     .setCommandPool(context_.getMainCommandPool())
                     .setLevel(vk::CommandBufferLevel::ePrimary)
                     .setCommandBufferCount(static_cast<uint32_t>(engineConfig::MAX_FRAMES_IN_FLIGHT));

    commandBuffers_ = context_.getDevice().allocateCommandBuffers(allocInfo);
}


void Renderer::recordCommandBuffer(const vk::CommandBuffer cmd,
                                   const uint32_t imageIndex,
                                   const UserInterface &userInterface) const {
    auto beginInfo = vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmd.begin(beginInfo);
    {
        prepareFrameImages(cmd, imageIndex);

        renderGraph_->execute(cmd, gpuTimestamps_.get(), currentFrame);

        // Overlay runs after the graph: depth is already eDepthReadOnlyOptimal (Lighting declared it),
        // swapchain color has Lighting's output. Overlay eLoads both — no barriers needed.
        const uint32_t overlaySlot = renderGraph_->passCount();
        vk_util::cmdBeginLabel(cmd, "Overlay");
        if (gpuTimestamps_) gpuTimestamps_->writeBegin(cmd, currentFrame, overlaySlot);
        overlayPass_->execute(cmd, *graphicsPipeline_, imageIndex,
                              descriptorSets_[currentFrame], sphereMesh_, currentInstanceCount_);
        if (gpuTimestamps_) gpuTimestamps_->writeEnd(cmd, currentFrame, overlaySlot);
        vk_util::cmdEndLabel(cmd);

        // UI pass (ImGui handles its own begin/endRendering)
        vk_util::cmdBeginLabel(cmd, "ImGui");
        userInterface.recordCommands(cmd, imageIndex);
        vk_util::cmdEndLabel(cmd);

        finalizeFrameImages(cmd, imageIndex);
    }
    cmd.end();
}

void Renderer::prepareFrameImages(vk::CommandBuffer cmd, uint32_t imageIndex) const {
    // Batch all frame-start layout transitions into a single pipelineBarrier2 call so the
    // driver can merge them, rather than issuing 2–4 separate sync points.
    const vk::ImageMemoryBarrier2 depthBarrier = vk::ImageMemoryBarrier2()
                                                 .setOldLayout(vk::ImageLayout::eUndefined)
                                                 .setNewLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
                                                 .setSrcStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                                                  vk::PipelineStageFlagBits2::eLateFragmentTests)
                                                 .setSrcAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
                                                 .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                                                  vk::PipelineStageFlagBits2::eLateFragmentTests)
                                                 .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentRead |
                                                                   vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
                                                 .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                                                 .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                                                 .setImage(swapChain_.getDepthImage())
                                                 .setSubresourceRange({vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1});

    std::array<vk::ImageMemoryBarrier2, 2> barriers = {
        vk_util::undefinedToColorAttachment(swapChain_.getImages()[imageIndex]),
        depthBarrier,
    };
    cmd.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(barriers));
}

void Renderer::finalizeFrameImages(vk::CommandBuffer cmd, uint32_t imageIndex) const {
    // Transition Swapchain: ColorAttachment -> PresentSource
    auto barrier = vk_util::colorAttachmentToPresent(swapChain_.getImages()[imageIndex]);
    cmd.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(barrier));
}

void Renderer::createSyncObjects() {
    auto device = context_.getDevice();
    const auto imageCount = static_cast<uint32_t>(swapChain_.getImageViews().size());

    // 1. Resize containers to match your logic
    inFlightFences_.resize(engineConfig::MAX_FRAMES_IN_FLIGHT);
    imageAvailableSemaphores_.resize(imageCount);
    renderFinishedSemaphores_.resize(imageCount);
    imagesInFlight.resize(imageCount, nullptr);

    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{};
    // Start signaled so the first frame doesn't block indefinitely
    fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);

    // 2. Create Fences (Per Frame Slot: MAX_FRAMES_IN_FLIGHT)
    for (size_t i = 0; i < engineConfig::MAX_FRAMES_IN_FLIGHT; i++) {
        inFlightFences_[i] = device.createFence(fenceInfo);
    }

    // 3. Create Semaphores (Per Swapchain Image: imageCount)
    for (size_t i = 0; i < imageCount; i++) {
        imageAvailableSemaphores_[i] = device.createSemaphore(semaphoreInfo);
        renderFinishedSemaphores_[i] = device.createSemaphore(semaphoreInfo);
    }
}

void Renderer::drawFrame(const GraphicsPipeline &graphicsPipeline,
                         const bool framebufferResized,
                         const Camera &camera,
                         const UserInterface &userInterface,
                         const std::vector<MeshInstance> &meshInstances,
                         const std::vector<PointLight> &pointLights,
                         const DirLightView &dirLight) {
    auto device = context_.getDevice();

    // 1. Wait for the Frame Slot to be free (CPU-GPU Sync)
    // Using (void) to acknowledge the Result, or let it throw on device loss
    (void)device.waitForFences(inFlightFences_[currentFrame], true, UINT64_MAX);

    // 2. Acquire Next Image
    // Note: We use the semaphore at [currentFrame] to signal acquisition
    uint32_t imageIndex;
    try {
        auto result = device.acquireNextImageKHR(swapChain_.getHandle(), UINT64_MAX,
                                                 imageAvailableSemaphores_[currentFrame], nullptr);
        imageIndex = result.value;
    } catch (const vk::OutOfDateKHRError &) {
        recreateSwapChain();
        return;
    }

    // 3. Handle Image-in-Flight Overlap (Old Logic)
    // If this specific image is still being used by a previous frame slot, wait for it.
    if (imagesInFlight[imageIndex]) {
        (void)device.waitForFences(imagesInFlight[imageIndex], true, UINT64_MAX);
    }
    // Mark this image as being used by the current frame's fence
    imagesInFlight[imageIndex] = inFlightFences_[currentFrame];

    // 4. Reset Fence and Record Commands
    device.resetFences(inFlightFences_[currentFrame]);

    // Read back GPU timestamps written by the previous submission of this frame slot.
    // Skip the first MAX_FRAMES_IN_FLIGHT frames: the pools haven't been written yet.
    if (gpuTimestamps_) {
        if (frameCount_ >= engineConfig::MAX_FRAMES_IN_FLIGHT) {
            const auto names = buildPassNameList();
            gpuTimestamps_->readback(currentFrame, names, static_cast<uint32_t>(names.size()));
        }
        gpuTimestamps_->resetPool(currentFrame);
    }
    ++frameCount_;

    updateUniformBuffer(currentFrame, camera, pointLights, dirLight);
    const uint32_t instanceCount = updateInstanceBuffer(currentFrame, meshInstances);

    graphicsPipeline_     = &graphicsPipeline;
    meshInstances_        = &meshInstances;
    dirLight_             = &dirLight;
    currentImageIndex_    = imageIndex;
    currentInstanceCount_ = instanceCount;

    commandBuffers_[currentFrame].reset();
    recordCommandBuffer(commandBuffers_[currentFrame], imageIndex, userInterface);

    // 5. Submit Info (Modern C++ Style)
    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

    auto submitInfo = vk::SubmitInfo()
                      .setWaitSemaphores(imageAvailableSemaphores_[currentFrame]) // Wait for Acquire
                      .setWaitDstStageMask(waitStages)
                      .setCommandBuffers(commandBuffers_[currentFrame])
                      .setSignalSemaphores(renderFinishedSemaphores_[imageIndex]); // Signal per IMAGE

    context_.getGraphicsQueue().submit(submitInfo, inFlightFences_[currentFrame]);

    // 6. Presentation Info
    vk::SwapchainKHR swapChainHandle = swapChain_.getHandle();
    auto presentInfo = vk::PresentInfoKHR()
                       .setWaitSemaphores(renderFinishedSemaphores_[imageIndex]) // Wait for render finished
                       .setSwapchains(swapChainHandle)
                       .setPImageIndices(&imageIndex);

    vk::Result presentResult;
    try {
        presentResult = context_.getPresentQueue().presentKHR(presentInfo);
    } catch (const vk::OutOfDateKHRError &) {
        presentResult = vk::Result::eErrorOutOfDateKHR;
    }

    // 7. Check for resize/recreation
    if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR ||
        framebufferResized) {
        recreateSwapChain();
    }

    // 8. Advance Frame Index
    currentFrame = (currentFrame + 1) % engineConfig::MAX_FRAMES_IN_FLIGHT;
}

void Renderer::recreateSwapChain() {
    // 1. Handle Minimization (Pause the engine if width/height is 0)
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    // 2. Synchronize: Stop the GPU before we delete its tools
    vkDeviceWaitIdle(context_.getDevice());

    // 3. Recreate SwapChain (updates images, views, and depth resources)
    swapChain_.recreate();

    // 4. Recreate G-buffer at the new resolution and re-point descriptor sets
    if (gbuffer_) {
        auto extent = swapChain_.getExtent();
        gbuffer_->recreate(extent.width, extent.height);
        updateLightingInputsDescSets();
    }

    // 5. Recreate extent-sized SSAO/blur images and re-point their descriptor sets.
    // The swapchain depth view is also new after recreate(), so both sets must be rewritten.
    ssaoPass_.reset();
    ssaoBlurPass_.reset();
    ssaoPass_ = std::make_unique<SsaoPass>(swapChain_, context_);
    ssaoBlurPass_ = std::make_unique<SsaoBlurPass>(swapChain_, context_);
    updateSsaoDescriptorSets();
    updateSsaoBlurDescriptorSet();

    // Re-register graph textures so barriers use the new image handles from the recreated resources.
    rebuildRenderGraph();

    // Note: Since we use Dynamic State for Viewport/Scissor,
    // we do NOT need to recreate the Pipeline!
}


void Renderer::createUniformBuffers() {
    vk::DeviceSize bufferSize = sizeof(GlobalUBO);

    uniformBuffers_.resize(engineConfig::MAX_FRAMES_IN_FLIGHT);
    uniformBuffersAllocation_.resize(engineConfig::MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped_.resize(engineConfig::MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < engineConfig::MAX_FRAMES_IN_FLIGHT; i++) {
        VmaAllocationInfo allocInfo;
        vk_util::createBuffer(context_.getVmaAllocator(), bufferSize,
                              vk::BufferUsageFlagBits::eUniformBuffer,
                              VMA_MEMORY_USAGE_CPU_TO_GPU,
                              uniformBuffers_[i], uniformBuffersAllocation_[i],
                              VMA_ALLOCATION_CREATE_MAPPED_BIT, &allocInfo);
        uniformBuffersMapped_[i] = allocInfo.pMappedData;
    }
}

static constexpr uint32_t MAX_INSTANCES = 64;

void Renderer::createInstanceBuffers() {
    const vk::DeviceSize bufferSize = MAX_INSTANCES * sizeof(InstanceData);

    instanceBuffers_.resize(engineConfig::MAX_FRAMES_IN_FLIGHT);
    instanceAllocations_.resize(engineConfig::MAX_FRAMES_IN_FLIGHT);
    instanceBuffersMapped_.resize(engineConfig::MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < engineConfig::MAX_FRAMES_IN_FLIGHT; i++) {
        VmaAllocationInfo allocInfo;
        vk_util::createBuffer(context_.getVmaAllocator(), bufferSize,
                              vk::BufferUsageFlagBits::eStorageBuffer,
                              VMA_MEMORY_USAGE_CPU_TO_GPU,
                              instanceBuffers_[i], instanceAllocations_[i],
                              VMA_ALLOCATION_CREATE_MAPPED_BIT, &allocInfo);
        instanceBuffersMapped_[i] = allocInfo.pMappedData;
    }
}

uint32_t Renderer::updateInstanceBuffer(const uint32_t currentFrame,
                                        const std::vector<MeshInstance> &meshInstances) const {
    if (!sphereMesh_)
        return 0;

    std::vector<InstanceData> instances;
    for (const auto &[mesh, transform, name, color] : meshInstances) {
        if (mesh == sphereMesh_)
            instances.push_back({transform.modelMatrix, color});
    }

    assert(instances.size() <= MAX_INSTANCES && "sphere instance count exceeds SSBO capacity");

    if (!instances.empty()) {
        std::memcpy(instanceBuffersMapped_[currentFrame], instances.data(),
                    instances.size() * sizeof(InstanceData));
    }

    return static_cast<uint32_t>(instances.size());
}


void Renderer::updateUniformBuffer(uint32_t currentImage, const Camera &camera,
                                   const std::vector<PointLight> &pointLights,
                                   const DirLightView &dirLight) const {
    GlobalUBO ubo{};
    ubo.view = camera.getViewMatrix();
    ubo.proj = camera.getProjectionMatrix(swapChain_.getExtent().width / (float)swapChain_.getExtent().height);
    ubo.invView = glm::inverse(ubo.view);
    ubo.invProj = glm::inverse(ubo.proj);
    ubo.cameraPos = glm::vec4(camera.position, 0.0f);
    ubo.dirLightSpaceMatrix = dirLight.lightSpaceMatrix();
    ubo.dirLightDir = glm::vec4(glm::normalize(dirLight.target - dirLight.position), 0.0f);

    const size_t count = std::min(pointLights.size(), size_t{24});
    for (size_t i = 0; i < count; ++i)
        ubo.pointLights[i] = pointLights[i];

    std::memcpy(uniformBuffersMapped_[currentImage], &ubo, sizeof(ubo));
}

void Renderer::createDescriptorPool() {
    // Pool Size 1: For the Global UBO (Set 0)
    constexpr auto uboPoolSize = vk::DescriptorPoolSize()
                                 .setType(vk::DescriptorType::eUniformBuffer)
                                 .setDescriptorCount(static_cast<uint32_t>(engineConfig::MAX_FRAMES_IN_FLIGHT + 1));

    // Pool Size 2: For the Instance SSBO (Set 0, binding 1)
    constexpr auto ssboPoolSize = vk::DescriptorPoolSize()
                                  .setType(vk::DescriptorType::eStorageBuffer)
                                  .setDescriptorCount(static_cast<uint32_t>(engineConfig::MAX_FRAMES_IN_FLIGHT));

    // Pool Size 3: For the Texture Samplers (Set 1) + G-buffer samplers (Set 2) + shadow map (binding 4)
    // Sponza has ~80 materials; G-buffer adds 4 per frame-in-flight; shadow map adds 1 per frame-in-flight
    constexpr auto samplerPoolSize = vk::DescriptorPoolSize()
                                     .setType(vk::DescriptorType::eCombinedImageSampler)
                                     .setDescriptorCount(200 + 5 * engineConfig::MAX_FRAMES_IN_FLIGHT + 1 + 2);

    std::array<vk::DescriptorPoolSize, 3> poolSizes = {uboPoolSize, ssboPoolSize, samplerPoolSize};
    auto poolInfo = vk::DescriptorPoolCreateInfo()
                    .setPoolSizeCount(poolSizes.size())
                    .setPoolSizes(poolSizes)
                    .setMaxSets(300);

    descriptorPool_ = context_.getDevice().createDescriptorPool(poolInfo);
}

void Renderer::createDescriptorSets() {
    std::vector<vk::DescriptorSetLayout> layouts(engineConfig::MAX_FRAMES_IN_FLIGHT, globalDescriptorSetLayout_);
    auto allocInfo = vk::DescriptorSetAllocateInfo()
                     .setDescriptorPool(descriptorPool_)
                     .setDescriptorSetCount(static_cast<uint32_t>(engineConfig::MAX_FRAMES_IN_FLIGHT))
                     .setPSetLayouts(layouts.data());

    descriptorSets_ = context_.getDevice().allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < engineConfig::MAX_FRAMES_IN_FLIGHT; i++) {
        auto uboInfo = vk::DescriptorBufferInfo()
                       .setBuffer(uniformBuffers_[i])
                       .setOffset(0)
                       .setRange(sizeof(GlobalUBO));

        auto ssboInfo = vk::DescriptorBufferInfo()
                        .setBuffer(instanceBuffers_[i])
                        .setOffset(0)
                        .setRange(64 * sizeof(InstanceData));

        std::array<vk::WriteDescriptorSet, 2> writes = {
            vk::WriteDescriptorSet()
            .setDstSet(descriptorSets_[i])
            .setDstBinding(0)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setDescriptorCount(1)
            .setPBufferInfo(&uboInfo),
            vk::WriteDescriptorSet()
            .setDstSet(descriptorSets_[i])
            .setDstBinding(1)
            .setDescriptorType(vk::DescriptorType::eStorageBuffer)
            .setDescriptorCount(1)
            .setPBufferInfo(&ssboInfo),
        };

        context_.getDevice().updateDescriptorSets(writes, nullptr);
    }
}

void Renderer::createLightingInputsDescSets() {
    // Allocate one G-buffer descriptor set per frame-in-flight
    std::vector<vk::DescriptorSetLayout> layouts(engineConfig::MAX_FRAMES_IN_FLIGHT, lightingInputsLayout_);
    auto allocInfo = vk::DescriptorSetAllocateInfo()
                     .setDescriptorPool(descriptorPool_)
                     .setSetLayouts(layouts);
    lightingInputsDescSets_ = context_.getDevice().allocateDescriptorSets(allocInfo);

    updateLightingInputsDescSets();
}

void Renderer::updateLightingInputsDescSets() {
    for (size_t i = 0; i < engineConfig::MAX_FRAMES_IN_FLIGHT; i++) {
        auto albedoInfo = vk::DescriptorImageInfo()
                          .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                          .setImageView(gbuffer_->getAlbedoMetallicView())
                          .setSampler(nearestClampSampler_);
        auto normalInfo = vk::DescriptorImageInfo()
                          .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                          .setImageView(gbuffer_->getNormalRoughnessView())
                          .setSampler(nearestClampSampler_);
        auto depthInfo = vk::DescriptorImageInfo()
                         .setImageLayout(vk::ImageLayout::eDepthReadOnlyOptimal)
                         .setImageView(swapChain_.getDepthImageView())
                         .setSampler(nearestClampSampler_);

        auto shadowInfo = vk::DescriptorImageInfo()
                          .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                          .setImageView(shadowMap_->getDepthView())
                          .setSampler(shadowMap_->getSampler());

        std::array<vk::WriteDescriptorSet, 4> writes = {
            vk_util::imageSamplerWrite(lightingInputsDescSets_[i], 0, albedoInfo),
            vk_util::imageSamplerWrite(lightingInputsDescSets_[i], 1, normalInfo),
            vk_util::imageSamplerWrite(lightingInputsDescSets_[i], 2, depthInfo),
            vk_util::imageSamplerWrite(lightingInputsDescSets_[i], 4, shadowInfo),
        };
        context_.getDevice().updateDescriptorSets(writes, nullptr);
    }
}

void Renderer::createSsaoDescriptorSets() {
    // Single static set — kernel and noise are immutable after upload
    auto allocInfo = vk::DescriptorSetAllocateInfo()
                     .setDescriptorPool(descriptorPool_)
                     .setSetLayouts(ssaoBufferLayout_);
    ssaoBufferDescriptorSet_ = context_.getDevice().allocateDescriptorSets(allocInfo)[0];
    updateSsaoDescriptorSets();
    // gbuffer binding 3 (ssao result for lighting) is written by createSsaoBlurDescriptorSet
    // after the blur pass is constructed, so it points to the blurred output.
}

void Renderer::updateSsaoDescriptorSets() {
    // Re-points the ssao buffer descriptor set to the current ssaoPass_ resources.
    // Called both at creation and on recreateSwapChain (ssaoPass_ is torn down and rebuilt).
    auto ssaoKernel = vk::DescriptorBufferInfo()
                      .setBuffer(ssaoPass_->getKernelBuffer())
                      .setOffset(0)
                      .setRange(sizeof(SSAOKernelUBO));

    auto ssaoNoise = vk::DescriptorImageInfo()
                     .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                     .setImageView(ssaoPass_->getSsaoNoiseImageView())
                     .setSampler(ssaoNoiseSampler_);

    std::array<vk::WriteDescriptorSet, 2> writes = {
        vk::WriteDescriptorSet()
        .setDstSet(ssaoBufferDescriptorSet_)
        .setDstBinding(0)
        .setDescriptorType(vk::DescriptorType::eUniformBuffer)
        .setDescriptorCount(1)
        .setPBufferInfo(&ssaoKernel),
        vk_util::imageSamplerWrite(ssaoBufferDescriptorSet_, 1, ssaoNoise),
    };
    context_.getDevice().updateDescriptorSets(writes, nullptr);
}

void Renderer::createSsaoBlurDescriptorSet() {
    auto allocInfo = vk::DescriptorSetAllocateInfo()
                     .setDescriptorPool(descriptorPool_)
                     .setSetLayouts(ssaoBlurLayout_);
    ssaoBlurDescriptorSet_ = context_.getDevice().allocateDescriptorSets(allocInfo)[0];
    updateSsaoBlurDescriptorSet();
}

void Renderer::updateSsaoBlurDescriptorSet() {
    // binding 0: depth texture
    auto depthInfo = vk::DescriptorImageInfo()
                     .setImageLayout(vk::ImageLayout::eDepthReadOnlyOptimal)
                     .setImageView(swapChain_.getDepthImageView())
                     .setSampler(nearestClampSampler_);

    // binding 1: raw SSAO output (pre-blur)
    auto ssaoInfo = vk::DescriptorImageInfo()
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setImageView(ssaoPass_->getSsaoKernelBufferImageView())
                    .setSampler(nearestClampSampler_);

    std::array<vk::WriteDescriptorSet, 2> writes = {
        vk_util::imageSamplerWrite(ssaoBlurDescriptorSet_, 0, depthInfo),
        vk_util::imageSamplerWrite(ssaoBlurDescriptorSet_, 1, ssaoInfo),
    };
    context_.getDevice().updateDescriptorSets(writes, nullptr);

    // Update gbuffer set binding 3 to point to the BLURRED output for the lighting pass
    for (size_t i = 0; i < engineConfig::MAX_FRAMES_IN_FLIGHT; i++) {
        auto blurredInfo = vk::DescriptorImageInfo()
                           .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                           .setImageView(ssaoBlurPass_->getBlurredImageView())
                           .setSampler(nearestClampSampler_);

        auto write = vk_util::imageSamplerWrite(lightingInputsDescSets_[i], 3, blurredInfo);
        context_.getDevice().updateDescriptorSets(write, nullptr);
    }
}

vk::DescriptorSet Renderer::createTextureDescriptorSet(
    const vk::ImageView imageView,
    const vk::ImageView normalView,
    const vk::ImageView metallicRoughnessView,
    const vk::Sampler sampler) {
    // 1. Allocate a single set using the Texture Layout (Set 1)
    const auto allocInfo = vk::DescriptorSetAllocateInfo()
                           .setDescriptorPool(descriptorPool_)
                           .setSetLayouts(textureLayout_); // This is the layout you created for Set 1

    // allocateDescriptorSets returns a vector; we just need the first one
    const vk::DescriptorSet textureSet = context_.getDevice().allocateDescriptorSets(allocInfo)[0];

    // 2. Update the set to point to the specific image/sampler
    const auto imageInfo = vk::DescriptorImageInfo()
                           .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                           .setImageView(imageView)
                           .setSampler(sampler);

    const auto normalInfo = vk::DescriptorImageInfo()
                            .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                            .setImageView(normalView)
                            .setSampler(sampler);

    const auto metalRoughInfo = vk::DescriptorImageInfo()
                                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                                .setImageView(metallicRoughnessView) // Pass this in
                                .setSampler(sampler);

    std::array<vk::WriteDescriptorSet, 3> descriptorWrites = {
        vk_util::imageSamplerWrite(textureSet, 0, imageInfo),
        vk_util::imageSamplerWrite(textureSet, 1, normalInfo),
        vk_util::imageSamplerWrite(textureSet, 2, metalRoughInfo),
    };
    context_.getDevice().updateDescriptorSets(descriptorWrites, nullptr);

    return textureSet;
}

void Renderer::createDescriptorSetLayout() {
    auto device = context_.getDevice();

    // --- [SET 0]: GLOBAL DATA (Camera/UBO + Instance SSBO) ---
    {
        std::array<vk::DescriptorSetLayoutBinding, 2> globalBindings = {
            // Binding 0: Camera UBO
            vk::DescriptorSetLayoutBinding()
            .setBinding(0)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setDescriptorCount(1)
            .setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment),
            // Binding 1: Instance SSBO
            vk::DescriptorSetLayoutBinding()
            .setBinding(1)
            .setDescriptorType(vk::DescriptorType::eStorageBuffer)
            .setDescriptorCount(1)
            .setStageFlags(vk::ShaderStageFlagBits::eVertex),
        };

        vk::DescriptorSetLayoutCreateInfo globalInfo({}, globalBindings);
        globalDescriptorSetLayout_ = device.createDescriptorSetLayout(globalInfo);
    }

    // --- [SET 1]: MATERIAL DATA (PBR Textures) ---
    {
        // Define bindings clearly with comments
        std::array<vk::DescriptorSetLayoutBinding, 3> pbrBindings = {
            // Binding 0: Albedo
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1,
                                           vk::ShaderStageFlagBits::eFragment),
            // Binding 1: Normal Map
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1,
                                           vk::ShaderStageFlagBits::eFragment),
            // Binding 2: Metallic-Roughness
            vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1,
                                           vk::ShaderStageFlagBits::eFragment)
        };

        vk::DescriptorSetLayoutCreateInfo materialInfo({}, pbrBindings);
        textureLayout_ = device.createDescriptorSetLayout(materialInfo);
    }

    // --- [SET 2]: G-BUFFER INPUTS (deferred lighting pass) ---
    {
        std::array<vk::DescriptorSetLayoutBinding, 5> gbufferBindings = {
            // Binding 0: Albedo + Metallic
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1,
                                           vk::ShaderStageFlagBits::eFragment),
            // Binding 1: Normal + Roughness
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1,
                                           vk::ShaderStageFlagBits::eFragment),
            // Binding 2: Depth (for world-position reconstruction)
            vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1,
                                           vk::ShaderStageFlagBits::eFragment),
            // Binding 3: SSAO blurred result
            vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1,
                                           vk::ShaderStageFlagBits::eFragment),
            // Binding 4: Directional shadow map
            // vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eCombinedImageSampler, 1,
            //                                vk::ShaderStageFlagBits::eFragment),
            // Binding 4: Directional shadow map with hardware PCF
            vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eCombinedImageSampler, 1,
                                           vk::ShaderStageFlagBits::eFragment),
        };

        vk::DescriptorSetLayoutCreateInfo gbufferInfo({}, gbufferBindings);
        lightingInputsLayout_ = device.createDescriptorSetLayout(gbufferInfo);
    }

    // --- [SET 3]: SSAO INPUTS (ssao pass) ---
    {
        std::array<vk::DescriptorSetLayoutBinding, 2> ssaoBindings = {
            // Binding 0: kernel
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1,
                                           vk::ShaderStageFlagBits::eFragment),
            // Binding 1: noise
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1,
                                           vk::ShaderStageFlagBits::eFragment),

        };

        vk::DescriptorSetLayoutCreateInfo ssaoBufferInfo({}, ssaoBindings);
        ssaoBufferLayout_ = device.createDescriptorSetLayout(ssaoBufferInfo);
    }

    // --- [SET for SSAO BLUR]: depth (binding 0) + raw SSAO (binding 1) ---
    // Separate layout — blur pipeline has its own VkPipelineLayout (set = 0).
    {
        std::array<vk::DescriptorSetLayoutBinding, 2> blurBindings = {
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1,
                                           vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1,
                                           vk::ShaderStageFlagBits::eFragment),
        };
        vk::DescriptorSetLayoutCreateInfo blurInfo({}, blurBindings);
        ssaoBlurLayout_ = device.createDescriptorSetLayout(blurInfo);
    }

    // --- G-buffer nearest sampler (no filtering — texels map 1:1 to pixels) ---
    {
        auto samplerInfo = vk::SamplerCreateInfo()
                           .setMagFilter(vk::Filter::eNearest)
                           .setMinFilter(vk::Filter::eNearest)
                           .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                           .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                           .setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
        nearestClampSampler_ = device.createSampler(samplerInfo);
    }
    {
        auto noiseInfo = vk::SamplerCreateInfo()
                         .setMagFilter(vk::Filter::eNearest)
                         .setMinFilter(vk::Filter::eNearest)
                         .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                         .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                         .setAddressModeW(vk::SamplerAddressMode::eRepeat);
        ssaoNoiseSampler_ = device.createSampler(noiseInfo);

    }
}


void Renderer::setupDefaultMaterial(const vk::ImageView whiteView,
                                    const vk::ImageView normalView,
                                    const vk::ImageView blackView,
                                    const vk::Sampler sampler) {
    defaultMaterial.name = "Renderer_Fallback";
    defaultMaterial.baseColorFactor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    // Create the descriptor set for the fallback
    defaultMaterial.textureSet = createTextureDescriptorSet(
        whiteView,
        normalView,
        blackView,
        sampler
        );
}