//
// Created by johnny on 12/29/25.
//

#include "Renderer.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <cstring>

#include "../common/Uniform.hpp"
#include "../common/InstanceData.hpp"
#include "UserInterface.hpp"
#include "../common/Vertex.hpp"
#include "common/Config.hpp"
#include "scene/Mesh.hpp"
#include "vulkan/GraphicsPipeline.hpp"
#include "common/PushConstantConstant.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/SwapChain.hpp"


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

    vkDestroyDescriptorPool(context_.getDevice(), descriptorPool_, nullptr);
    std::cerr << "[Destructor] Renderer-descriptorPool_..." << std::endl;

    vkDestroyDescriptorSetLayout(context_.getDevice(), globalDescriptorSetLayout_, nullptr);
    std::cerr << "[Destructor] Renderer-descriptorSetLayout_..." << std::endl;

    if (textureLayout_) {
        vkDestroyDescriptorSetLayout(context_.getDevice(), textureLayout_, nullptr);
        std::cerr << "[Destructor] Renderer-textureLayout_..." << std::endl;
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
    createUniformBuffers();
    createInstanceBuffers();
    createDescriptorPool();
    createDescriptorSets();
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
                                   const GraphicsPipeline &graphicsPipeline,
                                   const uint32_t imageIndex,
                                   const UserInterface &userInterface,
                                   const std::vector<MeshInstance> &meshInstances,
                                   const uint32_t instanceCount) const {
    // 1. Setup
    auto beginInfo = vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmd.begin(beginInfo);
    {
        // 2. Prepare Images (Transitions)
        prepareFrameImages(cmd, imageIndex);

        // 3. Main Geometry Pass
        renderScene(cmd, graphicsPipeline, imageIndex, meshInstances, instanceCount);

        // 4. UI Pass (ImGui)
        // Note: UserInterface handles its own begin/endRendering internally
        userInterface.recordCommands(cmd, imageIndex);

        // 5. Present Preparation
        finalizeFrameImages(cmd, imageIndex);
    }
    cmd.end();
}

void Renderer::renderScene(const vk::CommandBuffer cmd,
                           const GraphicsPipeline &graphicsPipeline,
                           const uint32_t imageIndex,
                           const std::vector<MeshInstance> &meshInstances,
                           const uint32_t instanceCount) const {

    auto colorAttachment = getPrimaryColorAttachment(imageIndex);
    auto depthAttachment = getPrimaryDepthAttachment();

    vk::RenderingInfo renderingInfo{};
    renderingInfo.setRenderArea({{0, 0}, swapChain_.getExtent()})
                 .setLayerCount(1)
                 .setColorAttachments(colorAttachment)
                 .setPDepthAttachment(&depthAttachment);

    const vk::PipelineLayout layout = graphicsPipeline.getPipelineLayout();

    cmd.beginRendering(renderingInfo);
    {
        // Dynamic State
        auto extent = swapChain_.getExtent();
        cmd.setViewport(0, vk::Viewport(0.0f, 0.0f,
                                        static_cast<float>(extent.width),
                                        static_cast<float>(extent.height),
                                        0.0f, 1.0f));
        cmd.setScissor(0, vk::Rect2D({0, 0}, extent));

        // Bind Global Uniforms (Camera View/Proj) — done once, layout is shared
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               layout,
                               DescriptorSets::GLOBAL_SET,
                               {descriptorSets_[currentFrame]},
                               {});

        vk::Pipeline currentPipeline = nullptr;

        // --- NON-INSTANCED PASS: everything except sphere instances ---
        for (const auto &[mesh, transform, name, color] : meshInstances) {
            if (!mesh)
                continue;
            if (sphereMesh_ && mesh == sphereMesh_)
                continue; // drawn in the instanced pass below

            vk::DeviceSize offsets[] = {0};
            cmd.bindVertexBuffers(0, {mesh->getVertexBuffer()}, offsets);
            cmd.bindIndexBuffer(mesh->getIndexBuffer(), 0, vk::IndexType::eUint32);

            const auto &materials = mesh->getMaterials();

            for (const auto &submesh : mesh->getSubmeshes()) {
                const auto &mat = (submesh.materialIndex >= 0)
                                      ? materials[submesh.materialIndex]
                                      : defaultMaterial;

                // --- SELECT PIPELINE ---
                const vk::Pipeline targetPipeline = mat.unlit
                                                        ? graphicsPipeline.getUnlitPipeline()
                                                        : graphicsPipeline.getPbrPipeline();
                if (targetPipeline != currentPipeline) {
                    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, targetPipeline);
                    currentPipeline = targetPipeline;
                }

                // --- PUSH CONSTANTS (Matrix + per-instance color * material factor) ---
                MeshPushConstants constants;
                constants.modelMatrix     = transform.modelMatrix;
                constants.baseColorFactor = color * mat.baseColorFactor;

                cmd.pushConstants<MeshPushConstants>(
                    layout,
                    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                    0,
                    constants);

                // --- BIND MATERIAL TEXTURES (SET 1) ---
                if (mat.textureSet) {
                    cmd.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics,
                        layout,
                        DescriptorSets::MATERIAL_SET,
                        {mat.textureSet},
                        {});
                }

                cmd.drawIndexed(submesh.indexCount, 1, submesh.firstIndex, 0, 0);
            }
        }

        // --- INSTANCED PASS: all sphere instances in one draw call ---
        if (sphereMesh_ && instanceCount > 0) {
            vk::DeviceSize offset = 0;
            cmd.bindVertexBuffers(0, {sphereMesh_->getVertexBuffer()}, {offset});
            cmd.bindIndexBuffer(sphereMesh_->getIndexBuffer(), 0, vk::IndexType::eUint32);

            const vk::Pipeline unlitPipeline = graphicsPipeline.getUnlitPipeline();
            if (unlitPipeline != currentPipeline) {
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, unlitPipeline);
            }

            for (const auto &submesh : sphereMesh_->getSubmeshes()) {
                cmd.drawIndexed(submesh.indexCount, instanceCount, submesh.firstIndex, 0, 0);
            }
        }
    }
    cmd.endRendering();
}

vk::RenderingAttachmentInfo Renderer::getPrimaryColorAttachment(uint32_t imageIndex) const {
    vk::RenderingAttachmentInfo colorAttachment;
    colorAttachment.setImageView(swapChain_.getImageViews()[imageIndex])
                   .setImageLayout(vk::ImageLayout::eAttachmentOptimal)
                   .setLoadOp(vk::AttachmentLoadOp::eClear)
                   .setStoreOp(vk::AttachmentStoreOp::eStore)
                   .setClearValue(vk::ClearColorValue(std::array<float, 4>{0.02f, 0.02f, 0.02f, 1.0f}));
    return colorAttachment;
}

vk::RenderingAttachmentInfo Renderer::getPrimaryDepthAttachment() const {
    vk::RenderingAttachmentInfo depthAttachment;
    depthAttachment.setImageView(swapChain_.getDepthImageView())
                   .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
                   .setLoadOp(vk::AttachmentLoadOp::eClear)
                   // .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                   .setStoreOp(vk::AttachmentStoreOp::eStore)
                   .setClearValue(vk::ClearDepthStencilValue(1.0f, 0));
    return depthAttachment;
}

void Renderer::prepareFrameImages(vk::CommandBuffer cmd, uint32_t imageIndex) const {
    // Transition Swapchain: Undefined -> ColorAttachment
    transitionImageLayout(cmd, swapChain_.getImages()[imageIndex],
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eColorAttachmentOptimal,
                          vk::ImageAspectFlagBits::eColor);

    // Transition Depth: Undefined -> DepthAttachment
    transitionImageLayout(cmd, swapChain_.getDepthImage(),
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eDepthStencilAttachmentOptimal,
                          vk::ImageAspectFlagBits::eDepth);
}

void Renderer::finalizeFrameImages(vk::CommandBuffer cmd, uint32_t imageIndex) const {
    // Transition Swapchain: ColorAttachment -> PresentSource
    transitionImageLayout(cmd, swapChain_.getImages()[imageIndex],
                          vk::ImageLayout::eColorAttachmentOptimal,
                          vk::ImageLayout::ePresentSrcKHR,
                          vk::ImageAspectFlagBits::eColor);
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
                         const std::vector<PointLight> &pointLights) {
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

    updateUniformBuffer(currentFrame, camera, pointLights);
    const uint32_t instanceCount = updateInstanceBuffer(currentFrame, meshInstances);

    commandBuffers_[currentFrame].reset();
    recordCommandBuffer(commandBuffers_[currentFrame], graphicsPipeline, imageIndex, userInterface, meshInstances,
                        instanceCount);

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

void Renderer::recreateSwapChain() const {
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

        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eUniformBuffer, // Changed to vk:: enum
                     VMA_MEMORY_USAGE_CPU_TO_GPU,
                     uniformBuffers_[i], // These are now vk::Buffer
                     uniformBuffersAllocation_[i],
                     VMA_ALLOCATION_CREATE_MAPPED_BIT,
                     &allocInfo);

        // Store the persistent pointer provided by the MAPPED flag
        uniformBuffersMapped_[i] = allocInfo.pMappedData;
    }
}

void Renderer::createInstanceBuffers() {
    constexpr uint32_t MAX_INSTANCES = 64;
    const vk::DeviceSize bufferSize  = MAX_INSTANCES * sizeof(InstanceData);

    instanceBuffers_.resize(engineConfig::MAX_FRAMES_IN_FLIGHT);
    instanceAllocations_.resize(engineConfig::MAX_FRAMES_IN_FLIGHT);
    instanceBuffersMapped_.resize(engineConfig::MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < engineConfig::MAX_FRAMES_IN_FLIGHT; i++) {
        VmaAllocationInfo allocInfo;
        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eStorageBuffer,
                     VMA_MEMORY_USAGE_CPU_TO_GPU,
                     instanceBuffers_[i],
                     instanceAllocations_[i],
                     VMA_ALLOCATION_CREATE_MAPPED_BIT,
                     &allocInfo);

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

    if (!instances.empty()) {
        std::memcpy(instanceBuffersMapped_[currentFrame], instances.data(),
                    instances.size() * sizeof(InstanceData));
    }

    return static_cast<uint32_t>(instances.size());
}

void Renderer::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, VmaMemoryUsage vmaUsage,
                            vk::Buffer &buffer, VmaAllocation &allocation, VmaAllocationCreateFlags vmaFlags,
                            VmaAllocationInfo *outAllocInfo) const {

    // Convert vk:: types to raw C structs for VMA
    VkBufferCreateInfo bufferInfo =
        vk::BufferCreateInfo().setSize(size).setUsage(usage).setSharingMode(vk::SharingMode::eExclusive);

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = vmaUsage;
    allocInfo.flags = vmaFlags;

    VkBuffer rawBuffer;
    if (vmaCreateBuffer(context_.getVmaAllocator(), &bufferInfo, &allocInfo, &rawBuffer, &allocation, outAllocInfo) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer with VMA!");
    }
    buffer = rawBuffer;
}

void Renderer::copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) const {
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
    allocInfo.setCommandPool(context_.getMainCommandPool());
    allocInfo.setCommandBufferCount(1);

    auto cmd = context_.getDevice().allocateCommandBuffers(allocInfo)[0];

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    cmd.begin(beginInfo);
    vk::BufferCopy copyRegion(0, 0, size);
    cmd.copyBuffer(srcBuffer, dstBuffer, copyRegion);
    cmd.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBuffers(cmd);

    context_.getGraphicsQueue().submit(submitInfo);
    context_.getGraphicsQueue().waitIdle(); // Simple sync for one-time transfer

    context_.getDevice().freeCommandBuffers(context_.getMainCommandPool(), cmd);
}

void Renderer::updateUniformBuffer(uint32_t currentImage, const Camera &camera,
                                    const std::vector<PointLight> &pointLights) const {
    GlobalUBO ubo{};
    ubo.view      = camera.getViewMatrix();
    ubo.proj      = camera.getProjectionMatrix(swapChain_.getExtent().width / (float)swapChain_.getExtent().height);
    ubo.cameraPos = glm::vec4(camera.position, 0.0f);

    const size_t count = std::min(pointLights.size(), size_t{24});
    for (size_t i = 0; i < count; ++i)
        ubo.pointLights[i] = pointLights[i];

    std::memcpy(uniformBuffersMapped_[currentImage], &ubo, sizeof(ubo));
}

void Renderer::createDescriptorPool() {
    // Pool Size 1: For the Global UBO (Set 0)
    constexpr auto uboPoolSize = vk::DescriptorPoolSize()
                                 .setType(vk::DescriptorType::eUniformBuffer)
                                 .setDescriptorCount(static_cast<uint32_t>(engineConfig::MAX_FRAMES_IN_FLIGHT));

    // Pool Size 2: For the Instance SSBO (Set 0, binding 1)
    constexpr auto ssboPoolSize = vk::DescriptorPoolSize()
                                  .setType(vk::DescriptorType::eStorageBuffer)
                                  .setDescriptorCount(static_cast<uint32_t>(engineConfig::MAX_FRAMES_IN_FLIGHT));

    // Pool Size 3: For the Texture Samplers (Set 1)
    // Sponza has ~80 materials; let's reserve 200
    constexpr auto samplerPoolSize = vk::DescriptorPoolSize()
                                     .setType(vk::DescriptorType::eCombinedImageSampler)
                                     .setDescriptorCount(200);

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

    // 4. Update the set with BOTH writes
    std::array<vk::WriteDescriptorSet, 3> descriptorWrites{};

    descriptorWrites[0] = vk::WriteDescriptorSet()
                          .setDstSet(textureSet)
                          .setDstBinding(0)
                          .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                          .setDescriptorCount(1)
                          .setPImageInfo(&imageInfo);

    descriptorWrites[1] = vk::WriteDescriptorSet()
                          .setDstSet(textureSet)
                          .setDstBinding(1) // <--- Normal Map Binding
                          .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                          .setDescriptorCount(1)
                          .setPImageInfo(&normalInfo);

    descriptorWrites[2] = vk::WriteDescriptorSet()
                          .setDstSet(textureSet)
                          .setDstBinding(2) // Binding 2: Metallic-Roughness
                          .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                          .setDescriptorCount(1)
                          .setPImageInfo(&metalRoughInfo);

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
}

void Renderer::transitionImageLayout(const vk::CommandBuffer cmd,
                                     const vk::Image image,
                                     const vk::ImageLayout oldLayout,
                                     const vk::ImageLayout newLayout,
                                     const vk::ImageAspectFlags aspectMask) const {
    vk::ImageMemoryBarrier2 barrier;
    barrier.setOldLayout(oldLayout)
           .setNewLayout(newLayout)
           .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setImage(image)
           .setSubresourceRange(vk::ImageSubresourceRange(aspectMask, 0, 1, 0, 1));

    // Define pipeline stages and access masks based on layouts
    if (newLayout == vk::ImageLayout::eColorAttachmentOptimal) {
        barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
               .setSrcAccessMask(vk::AccessFlagBits2::eNone)
               .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
               .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite);
    } else if (newLayout == vk::ImageLayout::ePresentSrcKHR) {
        barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
               .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
               .setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe)
               .setDstAccessMask(vk::AccessFlagBits2::eNone);
    } else if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                vk::PipelineStageFlagBits2::eLateFragmentTests)
               .setSrcAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
               .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                vk::PipelineStageFlagBits2::eLateFragmentTests)
               .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentRead |
                                 vk::AccessFlagBits2::eDepthStencilAttachmentWrite);
    }

    vk::DependencyInfo dependencyInfo;
    dependencyInfo.setImageMemoryBarriers(barrier);

    cmd.pipelineBarrier2(dependencyInfo);
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