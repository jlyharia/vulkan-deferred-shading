//
// Created by johnny on 12/29/25.
//


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "renderer.hpp"

#include <chrono>
#include <cstring>

#include "Uniform.hpp"
#include "Vertex.hpp"
#include "common/config.hpp"
#include "vulkan/render_pass.hpp"
#include "vulkan/swap_chain.hpp"
#include "vulkan/VulkanContext.hpp"
// The C++ Bindings Header

Renderer::Renderer(VulkanContext &context, SwapChain &swapChain, RenderPass &renderPass,
                   GLFWwindow *window_)
    : context_(context), swapChain_(swapChain), renderPass_(renderPass),
      window_(window_) {

    // 1. Initialize Memory Allocator
    createAllocator();

    // 2. Initialize Command Infrastructure
    createCommandPool();
    createCommandBuffers();

    // 3. Setup Synchronization (Fences/Semaphores)
    createSyncObjects();
}

/**
* In Vulkan, the specific order of destruction between a Semaphore and a Command Pool does not technically matter, as long as they are both destroyed after the GPU has finished using them.
*
* However, there is a "Logical Best Practice" that most engine developers follow to keep code clean and mirror the creation order.
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

    vkDestroyDescriptorSetLayout(context_.getDevice(), descriptorSetLayout_, nullptr);
    std::cerr << "[Destructor] Renderer-descriptorSetLayout_..." << std::endl;
    for (size_t i = 0; i < engine::MAX_FRAMES_IN_FLIGHT; i++) {
        // VMA automatically handles the Unmapping if you used
        // VMA_ALLOCATION_CREATE_MAPPED_BIT.
        if (uniformBuffers_[i] != VK_NULL_HANDLE) {
            vmaDestroyBuffer(vmaAllocator, uniformBuffers_[i], uniformBuffersAllocation_[i]);

            // Safety: Clear the handles
            uniformBuffers_[i] = VK_NULL_HANDLE;
            uniformBuffersAllocation_[i] = nullptr;
            uniformBuffersMapped_[i] = nullptr;
        }
    }

    // This replaces BOTH vkDestroyBuffer and vkFreeMemory
    if (vertexBuffer_ != VK_NULL_HANDLE) {
        // This frees BOTH the buffer and the memory allocation
        vmaDestroyBuffer(vmaAllocator, vertexBuffer_, vertexBufferAllocation_);
        // Safety: set to null so you don't accidentally try to use it again
        vertexBuffer_ = VK_NULL_HANDLE;
        vertexBufferAllocation_ = nullptr;
    }

    if (indexBuffer_ != VK_NULL_HANDLE) {
        // This frees BOTH the buffer and the memory allocation
        vmaDestroyBuffer(vmaAllocator, indexBuffer_, indexBufferAllocation_);
        // Safety: set to null so you don't accidentally try to use it again
        indexBuffer_ = VK_NULL_HANDLE;
        indexBufferAllocation_ = nullptr;
    }
    // 3. Destroy the allocator itself
    // Note: All VMA buffers MUST be destroyed before this call
    if (vmaAllocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(vmaAllocator);
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

    // 4. Destroy Command Pool (Implicitly frees all Command Buffers)
    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(context_.getDevice(), commandPool_, nullptr);
    }
}

void Renderer::initResources(vk::PipelineLayout pipelineLayout, std::string modelPath) {
    activePipelineLayout_ = pipelineLayout;

    // Load model using your system
    ms.loadObjModel(modelPath);

    // Create resources using the helper we just built
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
}


void Renderer::createCommandPool() {
    auto queueFamilyIndices = context_.findQueueFamilies(context_.getPhysicalDevice());

    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT allows us to reuse command buffers every frame
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    poolInfo.setQueueFamilyIndex(queueFamilyIndices.graphicsFamily.value());

    commandPool_ = context_.getDevice().createCommandPool(poolInfo);
}

void Renderer::createCommandBuffers() {
    commandBuffers_.resize(engine::MAX_FRAMES_IN_FLIGHT);

    auto allocInfo = vk::CommandBufferAllocateInfo()
                     .setCommandPool(commandPool_)
                     .setLevel(vk::CommandBufferLevel::ePrimary)
                     .setCommandBufferCount(static_cast<uint32_t>(engine::MAX_FRAMES_IN_FLIGHT));

    commandBuffers_ = context_.getDevice().allocateCommandBuffers(allocInfo);
}


void Renderer::recordCommandBuffer(vk::CommandBuffer commandBuffer, vk::Pipeline pipeline, uint32_t imageIndex) const {
    auto beginInfo = vk::CommandBufferBeginInfo();
    commandBuffer.begin(beginInfo);

    std::array<vk::ClearValue, 2> clearValues;
    clearValues[0].color = vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

    auto renderPassInfo = vk::RenderPassBeginInfo()
                          .setRenderPass(renderPass_.getRenderPass())
                          .setFramebuffer(swapChain_.getFramebuffers()[imageIndex])
                          .setRenderArea(vk::Rect2D({0, 0}, swapChain_.getExtent()))
                          .setClearValueCount(static_cast<uint32_t>(clearValues.size()))
                          .setPClearValues(clearValues.data());

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    {
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

        // Set Dynamic Viewport/Scissor
        auto extent = swapChain_.getExtent();
        commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f));
        commandBuffer.setScissor(0, vk::Rect2D({0, 0}, extent));

        commandBuffer.bindVertexBuffers(0, {vertexBuffer_}, {0});
        commandBuffer.bindIndexBuffer(indexBuffer_, 0, vk::IndexType::eUint32);

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                         activePipelineLayout_, 0,
                                         {descriptorSets_[currentFrame]}, {});

        int shadingMode = 1;
        commandBuffer.pushConstants<int>(activePipelineLayout_, vk::ShaderStageFlagBits::eFragment, 0, shadingMode);

        commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
    }
    commandBuffer.endRenderPass();
    commandBuffer.end();
}


void Renderer::createSyncObjects() {
    auto device = context_.getDevice();
    const auto imageCount = static_cast<uint32_t>(swapChain_.getImageViews().size());

    // 1. Resize containers to match your logic
    inFlightFences_.resize(engine::MAX_FRAMES_IN_FLIGHT);
    imageAvailableSemaphores_.resize(imageCount);
    renderFinishedSemaphores_.resize(imageCount);
    imagesInFlight.resize(imageCount, nullptr);

    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{};
    // Start signaled so the first frame doesn't block indefinitely
    fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);

    // 2. Create Fences (Per Frame Slot: MAX_FRAMES_IN_FLIGHT)
    for (size_t i = 0; i < engine::MAX_FRAMES_IN_FLIGHT; i++) {
        inFlightFences_[i] = device.createFence(fenceInfo);
    }

    // 3. Create Semaphores (Per Swapchain Image: imageCount)
    for (size_t i = 0; i < imageCount; i++) {
        imageAvailableSemaphores_[i] = device.createSemaphore(semaphoreInfo);
        renderFinishedSemaphores_[i] = device.createSemaphore(semaphoreInfo);
    }
}


void Renderer::drawFrame(vk::Pipeline pipeline, bool framebufferResized, const Camera &camera) {
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
    recordCommandBuffer(commandBuffers_[currentFrame], pipeline, imageIndex);

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
    currentFrame = (currentFrame + 1) % engine::MAX_FRAMES_IN_FLIGHT;
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
    swapChain_.recreate(renderPass_.getRenderPass());

    // 5. Recreate Renderer resources with the NEW extent
    // createDepthResources();

    // Note: Since we use Dynamic State for Viewport/Scissor,
    // we do NOT need to recreate the Pipeline!
}


void Renderer::createVertexBuffer() {
    // auto& vertices = ms .getVertices();
    vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    // 1. Create Staging Buffer (CPU Visible)
    vk::Buffer stagingBuffer;
    VmaAllocation stagingAlloc;
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                 VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingAlloc);

    // 2. Map and Copy
    void *data;
    vmaMapMemory(vmaAllocator, stagingAlloc, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vmaUnmapMemory(vmaAllocator, stagingAlloc);

    // 3. Create GPU Local Buffer
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                 VMA_MEMORY_USAGE_GPU_ONLY, vertexBuffer_, vertexBufferAllocation_);

    // 4. Copy to GPU
    copyBuffer(stagingBuffer, vertexBuffer_, bufferSize);

    // 5. Cleanup Staging
    vmaDestroyBuffer(vmaAllocator, stagingBuffer, stagingAlloc);
}

void Renderer::createIndexBuffer() {
    vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    vk::Buffer stagingBuffer;
    VmaAllocation stagingAlloc;
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                 VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingAlloc);

    void *data;
    vmaMapMemory(vmaAllocator, stagingAlloc, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vmaUnmapMemory(vmaAllocator, stagingAlloc);

    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                 VMA_MEMORY_USAGE_GPU_ONLY, indexBuffer_, indexBufferAllocation_);

    copyBuffer(stagingBuffer, indexBuffer_, bufferSize);
    vmaDestroyBuffer(vmaAllocator, stagingBuffer, stagingAlloc);
}

void Renderer::createUniformBuffers() {
    vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers_.resize(engine::MAX_FRAMES_IN_FLIGHT);
    uniformBuffersAllocation_.resize(engine::MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped_.resize(engine::MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < engine::MAX_FRAMES_IN_FLIGHT; i++) {
        VmaAllocationInfo allocInfo;

        createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eUniformBuffer, // Changed to vk:: enum
            VMA_MEMORY_USAGE_CPU_TO_GPU,
            uniformBuffers_[i], // These are now vk::Buffer
            uniformBuffersAllocation_[i],
            VMA_ALLOCATION_CREATE_MAPPED_BIT,
            &allocInfo
            );

        // Store the persistent pointer provided by the MAPPED flag
        uniformBuffersMapped_[i] = allocInfo.pMappedData;
    }
}

void Renderer::createBuffer(vk::DeviceSize size,
                            vk::BufferUsageFlags usage,
                            VmaMemoryUsage vmaUsage,
                            vk::Buffer &buffer,
                            VmaAllocation &allocation,
                            VmaAllocationCreateFlags vmaFlags,
                            VmaAllocationInfo *outAllocInfo) const {

    // Convert vk:: types to raw C structs for VMA
    VkBufferCreateInfo bufferInfo = vk::BufferCreateInfo()
                                    .setSize(size)
                                    .setUsage(usage)
                                    .setSharingMode(vk::SharingMode::eExclusive);

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = vmaUsage;
    allocInfo.flags = vmaFlags;

    VkBuffer rawBuffer;
    if (vmaCreateBuffer(vmaAllocator, &bufferInfo, &allocInfo, &rawBuffer, &allocation, outAllocInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer with VMA!");
    }
    buffer = rawBuffer;
}

void Renderer::copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) const {
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
    allocInfo.setCommandPool(commandPool_);
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

    context_.getDevice().freeCommandBuffers(commandPool_, cmd);
}

void Renderer::createAllocator() {
    VmaVulkanFunctions vulkanFunctions{};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.instance = context_.getInstance();
    allocatorInfo.physicalDevice = context_.getPhysicalDevice();
    allocatorInfo.device = context_.getDevice();
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;

    VkResult res = vmaCreateAllocator(&allocatorInfo, &vmaAllocator);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("failed to create VMA allocator!");
    }
    VmaAllocatorInfo info{};
    vmaGetAllocatorInfo(vmaAllocator, &info);
    assert(info.device == context_.getDevice());
}

void Renderer::updateUniformBuffer(uint32_t currentImage, const Camera &camera) const {
    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = camera.getViewMatrix();
    ubo.proj = camera.getProjectionMatrix(swapChain_.getExtent().width / (float)swapChain_.getExtent().height);

    std::memcpy(uniformBuffersMapped_[currentFrame], &ubo, sizeof(ubo));
}


void Renderer::createDescriptorPool() {
    auto poolSize = vk::DescriptorPoolSize()
                    .setType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(static_cast<uint32_t>(engine::MAX_FRAMES_IN_FLIGHT));

    auto poolInfo = vk::DescriptorPoolCreateInfo()
                    .setPoolSizeCount(1)
                    .setPPoolSizes(&poolSize)
                    .setMaxSets(static_cast<uint32_t>(engine::MAX_FRAMES_IN_FLIGHT));

    descriptorPool_ = context_.getDevice().createDescriptorPool(poolInfo);
}

void Renderer::createDescriptorSets() {
    std::vector<vk::DescriptorSetLayout> layouts(engine::MAX_FRAMES_IN_FLIGHT, descriptorSetLayout_);
    auto allocInfo = vk::DescriptorSetAllocateInfo()
                     .setDescriptorPool(descriptorPool_)
                     .setDescriptorSetCount(static_cast<uint32_t>(engine::MAX_FRAMES_IN_FLIGHT))
                     .setPSetLayouts(layouts.data());

    descriptorSets_ = context_.getDevice().allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < engine::MAX_FRAMES_IN_FLIGHT; i++) {
        auto bufferInfo = vk::DescriptorBufferInfo()
                          .setBuffer(uniformBuffers_[i])
                          .setOffset(0)
                          .setRange(sizeof(UniformBufferObject));

        auto descriptorWrite = vk::WriteDescriptorSet()
                               .setDstSet(descriptorSets_[i])
                               .setDstBinding(0)
                               .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                               .setDescriptorCount(1)
                               .setPBufferInfo(&bufferInfo);

        context_.getDevice().updateDescriptorSets(descriptorWrite, nullptr);
    }
}

void Renderer::createDescriptorSetLayout() {
    auto uboLayoutBinding = vk::DescriptorSetLayoutBinding()
                            .setBinding(0)
                            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                            .setDescriptorCount(1)
                            .setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

    auto layoutInfo = vk::DescriptorSetLayoutCreateInfo()
                      .setBindingCount(1)
                      .setPBindings(&uboLayoutBinding);

    descriptorSetLayout_ = context_.getDevice().createDescriptorSetLayout(layoutInfo);
}