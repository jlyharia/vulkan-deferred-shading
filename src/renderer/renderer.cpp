//
// Created by johnny on 12/29/25.
//

#include "renderer.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <cstring>

#include "../common/Uniform.hpp"
#include "UserInterface.hpp"
#include "../common/Vertex.hpp"
#include "common/config.hpp"
#include "../scene/Model.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/swap_chain.hpp"
#include "vulkan/VulkanUtils.hpp"


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
                                   const vk::Pipeline pipeline,
                                   const uint32_t imageIndex,
                                   const UserInterface &userInterface,
                                   const std::vector<RenderObject> &renderObjs,
                                   const vk::PipelineLayout activePipelineLayout) const {
    // 1. Setup
    auto beginInfo = vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmd.begin(beginInfo);
    {
        // 2. Prepare Images (Transitions)
        prepareFrameImages(cmd, imageIndex);

        // 3. Main Geometry Pass
        renderScene(cmd, pipeline, imageIndex, renderObjs, activePipelineLayout);

        // 4. UI Pass (ImGui)
        // Note: UserInterface handles its own begin/endRendering internally
        userInterface.recordCommands(cmd, imageIndex);

        // 5. Present Preparation
        finalizeFrameImages(cmd, imageIndex);
    }
    cmd.end();
}

void Renderer::renderScene(const vk::CommandBuffer cmd,
                           const vk::Pipeline pipeline,
                           const uint32_t imageIndex,
                           const std::vector<RenderObject> &objects,
                           const vk::PipelineLayout activePipelineLayout) const {

    // Create attachment info using helpers to keep this clean
    auto colorAttachment = getPrimaryColorAttachment(imageIndex);
    auto depthAttachment = getPrimaryDepthAttachment();

    vk::RenderingInfo renderingInfo{};
    renderingInfo.setRenderArea({{0, 0}, swapChain_.getExtent()})
                 .setLayerCount(1)
                 .setColorAttachments(colorAttachment)
                 .setPDepthAttachment(&depthAttachment);

    cmd.beginRendering(renderingInfo);
    {
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

        // Dynamic State
        auto extent = swapChain_.getExtent();
        cmd.setViewport(0, vk::Viewport(0.0f, 0.0f,
                                        static_cast<float>(extent.width),
                                        static_cast<float>(extent.height),
                                        0.0f, 1.0f));
        cmd.setScissor(0, vk::Rect2D({0, 0}, extent));

        // Bind Global Uniforms (Camera View/Proj) - Done ONCE per frame
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               activePipelineLayout,
                               0, // This is Set 0 in your shader
                               {descriptorSets_[currentFrame]},
                               {});

        // --- THE OBJECT LOOP ---
        for (const auto &[model, transform, name] : objects) {
            if (!model)
                continue;

            // 1. Push the Model Matrix (Transform) to the Shader
            cmd.pushConstants<glm::mat4>(
                activePipelineLayout,
                vk::ShaderStageFlagBits::eVertex,
                0,
                transform.modelMatrix
                );

            // 2. Bind the Model's GPU Buffers
            vk::DeviceSize offsets[] = {0};
            cmd.bindVertexBuffers(0, {model->getVertexBuffer()}, offsets);
            cmd.bindIndexBuffer(model->getIndexBuffer(), 0, vk::IndexType::eUint32);

            // 3. DRAW SUBMESHES (The Material Loop)
            const auto &materials = model->getMaterials();

            // if (!materials.empty() && materials[0].textureSet) {
            //     cmd.bindDescriptorSets(
            //         vk::PipelineBindPoint::eGraphics,
            //         activePipelineLayout,
            //         1,
            //         1,
            //         &materials[0].textureSet, // Always bind the first material
            //         0, nullptr
            //     );
            // }
            for (const auto &submesh : model->getSubmeshes()) {

                // --- NEW: BIND MATERIAL (SET 1) ---
                if (submesh.materialIndex >= 0 && submesh.materialIndex < materials.size()) {
                    const auto &mat = materials[submesh.materialIndex];

                    // Only bind if a valid descriptor set exists for this material
                    if (mat.textureSet) {
                        cmd.bindDescriptorSets(
                            vk::PipelineBindPoint::eGraphics,
                            activePipelineLayout,
                            1, // This is Set 1 in your shader
                            1, // Binding 1 set
                            &mat.textureSet, // Pointer to the handle
                            0, nullptr // No dynamic offsets
                            );
                    }
                }

                // 4. Draw the specific index range for this material
                cmd.drawIndexed(submesh.indexCount, 1, submesh.firstIndex, 0, 0);
                // cmd.drawIndexed(submesh.indexCount, 1, submesh.firstIndex, 0, 0);
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
                   .setStoreOp(vk::AttachmentStoreOp::eDontCare)
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

void Renderer::drawFrame(const vk::Pipeline pipeline,
                         const bool framebufferResized,
                         const Camera &camera,
                         const UserInterface &userInterface,
                         const std::vector<RenderObject> &renderObjects,
                         const vk::PipelineLayout activePipelineLayout) {
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

    updateUniformBuffer(currentFrame, camera);

    commandBuffers_[currentFrame].reset();
    recordCommandBuffer(commandBuffers_[currentFrame], pipeline, imageIndex, userInterface, renderObjects,
                        activePipelineLayout);

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

    // 3. Cleanup size-dependent resources
    // cleanupDepthResources();
    // Framebuffers are cleaned inside SwapChain::cleanup() which we trigger next

    // 4. Recreate SwapChain (This updates images and views)
    swapChain_.recreate();

    // 5. Recreate Renderer resources with the NEW extent
    // createDepthResources();

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

void Renderer::updateUniformBuffer(uint32_t currentImage, const Camera &camera) const {
    GlobalUBO ubo{
        .view = camera.getViewMatrix(),
        .proj = camera.getProjectionMatrix(swapChain_.getExtent().width / (float)swapChain_.getExtent().height),
        .cameraPos = camera.position
    };
    std::memcpy(uniformBuffersMapped_[currentFrame], &ubo, sizeof(ubo));
}

void Renderer::createDescriptorPool() {
    // Pool Size 1: For the Global UBO (Set 0)
    constexpr auto uboPoolSize = vk::DescriptorPoolSize()
                                 .setType(vk::DescriptorType::eUniformBuffer)
                                 .setDescriptorCount(static_cast<uint32_t>(engineConfig::MAX_FRAMES_IN_FLIGHT));

    // Pool Size 2: For the Texture Samplers (Set 1)
    // Sponza has ~80 materials; let's reserve 200
    constexpr auto samplerPoolSize = vk::DescriptorPoolSize()
                                     .setType(vk::DescriptorType::eCombinedImageSampler)
                                     .setDescriptorCount(200);

    std::array<vk::DescriptorPoolSize, 2> poolSizes = {uboPoolSize, samplerPoolSize};
    // samplerPoolSize.descriptorCount +
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
        auto bufferInfo =
            vk::DescriptorBufferInfo()
            .setBuffer(uniformBuffers_[i])
            .setOffset(0)
            .setRange(sizeof(GlobalUBO));

        auto descriptorWrite = vk::WriteDescriptorSet()
                               .setDstSet(descriptorSets_[i])
                               .setDstBinding(0)
                               .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                               .setDescriptorCount(1)
                               .setPBufferInfo(&bufferInfo);

        context_.getDevice().updateDescriptorSets(descriptorWrite, nullptr);
    }
}

vk::DescriptorSet Renderer::createTextureDescriptorSet(
    const vk::ImageView imageView,
    const vk::Sampler sampler) {
    // 1. Allocate a single set using the Texture Layout (Set 1)
    auto allocInfo = vk::DescriptorSetAllocateInfo()
                     .setDescriptorPool(descriptorPool_)
                     .setSetLayouts(textureLayout_); // This is the layout you created for Set 1

    // allocateDescriptorSets returns a vector; we just need the first one
    const vk::DescriptorSet textureSet = context_.getDevice().allocateDescriptorSets(allocInfo)[0];

    // 2. Update the set to point to the specific image/sampler
    const auto imageInfo = vk::DescriptorImageInfo()
                           .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                           .setImageView(imageView)
                           .setSampler(sampler);

    const auto descriptorWrite = vk::WriteDescriptorSet()
                                 .setDstSet(textureSet)
                                 .setDstBinding(0) // Binding 0 in Set 1
                                 .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                                 .setDescriptorCount(1)
                                 .setPImageInfo(&imageInfo);

    context_.getDevice().updateDescriptorSets(descriptorWrite, nullptr);

    return textureSet;
}

void Renderer::createDescriptorSetLayout() {
    // Set 0: Global UBO (Camera)
    auto uboLayoutBinding = vk::DescriptorSetLayoutBinding()
                            .setBinding(0)
                            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                            .setDescriptorCount(1)
                            .setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

    // Set 1: Texture Sampler (Sponza Materials)
    auto samplerLayoutBinding = vk::DescriptorSetLayoutBinding()
                                .setBinding(0)
                                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                                .setDescriptorCount(1)
                                .setStageFlags(vk::ShaderStageFlagBits::eFragment);

    // Create both layouts and store them as members
    globalDescriptorSetLayout_ = context_.getDevice().createDescriptorSetLayout({{}, 1, &uboLayoutBinding});
    textureLayout_ = context_.getDevice().createDescriptorSetLayout({{}, 1, &samplerLayoutBinding});
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
    }

    vk::DependencyInfo dependencyInfo;
    dependencyInfo.setImageMemoryBarriers(barrier);

    cmd.pipelineBarrier2(dependencyInfo);
}

GltfModel Renderer::uploadModel(const GltfLoader::ModelData &data) const {
    GltfModel model;
    model.allocator = context_.getVmaAllocator();

    // We grab the pool from the Context (as we refactored)
    vk::CommandPool uploadPool = context_.getTransferCommandPool();

    // The template automatically knows T is Vertex and calculates the size!
    vk_util::uploadToDeviceBuffer(
        model.allocator,
        context_.getDevice(),
        context_.getGraphicsQueue(),
        uploadPool,
        data.vertices,
        vk::BufferUsageFlagBits::eVertexBuffer,
        model.vertexBuffer,
        model.vertexAllocation
        );

    vk_util::uploadToDeviceBuffer(
        model.allocator,
        context_.getDevice(),
        context_.getGraphicsQueue(),
        uploadPool,
        data.indices,
        vk::BufferUsageFlagBits::eIndexBuffer,
        model.indexBuffer,
        model.indexAllocation
        );

    return model;
}