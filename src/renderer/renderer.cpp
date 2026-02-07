//
// Created by johnny on 12/29/25.
//


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "renderer.hpp"

#include <chrono>
#include <cstring>
#include <iostream>

#include "Uniform.hpp"
#include "Vertex.hpp"
#include "common/config.hpp"
#include "vulkan/render_pass.hpp"
#include "vulkan/swap_chain.hpp"
#include "vulkan/VulkanContext.hpp"
// The C++ Bindings Header
#include <vulkan/vulkan.hpp>

Renderer::Renderer(VulkanContext &context, SwapChain &swapChain, RenderPass &renderPass,
                   GLFWwindow *window_)
    : context_(context), swapChain_(swapChain), renderPass_(renderPass),
      window_(window_) {
    // 1. Core Memory & Commands
    createAllocator(); // VMA should be first
    createCommandPool();
    createCommandBuffers();
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
    vkDeviceWaitIdle(context_.getDevice());

    vkDestroyDescriptorPool(context_.getDevice(), descriptorPool_, nullptr);

    vkDestroyDescriptorSetLayout(context_.getDevice(), descriptorSetLayout_, nullptr);
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

void Renderer::initResources(VkPipelineLayout pipelineLayout, std::string modelPath) {
    activePipelineLayout_ = pipelineLayout; // Store for vkCmdBindDescriptorSets
    ms.loadObjModel(modelPath);
    std::cout << "Vertices loaded: " << vertices.size() << std::endl;
    if (vertices.empty()) {
        throw std::runtime_error("No vertices loaded! Check your file path.");
    }
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
}

void Renderer::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = context_.findQueueFamilies(
        context_.getPhysicalDevice());

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(context_.getDevice(), &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

void Renderer::createCommandBuffers() {
    commandBuffers_.resize(engine::MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(engine::MAX_FRAMES_IN_FLIGHT);

    if (vkAllocateCommandBuffers(context_.getDevice(), &allocInfo, commandBuffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void Renderer::recordCommandBuffer(VkCommandBuffer commandBuffer, VkPipeline pipeline, uint32_t imageIndex) const {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_.getRenderPass();
    renderPassInfo.framebuffer = swapChain_.getFramebuffers()[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChain_.getExtent();

    std::array<VkClearValue, 2> clearValues{};
    // Index 0: Color (Black with 1.0 Alpha)
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    // Index 1: Depth (1.0 is the far plane, 0 is the stencil)
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();
    // begin render pass
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapChain_.getExtent().width;
        viewport.height = (float)swapChain_.getExtent().height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChain_.getExtent();
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        VkBuffer vertexBuffers[] = {vertexBuffer_};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipelineLayout_, 0, 1,
                                &descriptorSets_[currentFrame], 0, nullptr);

        // Inside your command buffer recording loop
        int shadingMode = 1; //usePhong ? 1 : 0; // Your toggle logic

        vkCmdPushConstants(
            commandBuffer,
            activePipelineLayout_, // Must match the layout used to create the pipeline
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0, // Offset
            sizeof(int), // Size
            &shadingMode // Pointer to the data
            );
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
    }
    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void Renderer::createSyncObjects() {
    const auto imageCount = static_cast<uint32_t>(swapChain_.getImages().size());

    // 1. Resize all containers
    inFlightFences_.resize(engine::MAX_FRAMES_IN_FLIGHT);
    imageAvailableSemaphores_.resize(imageCount);
    renderFinishedSemaphores_.resize(imageCount);
    imagesInFlight.resize(imageCount, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    // 2. Create Fences (Per Frame Slot)
    for (size_t i = 0; i < engine::MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateFence(context_.getDevice(), &fenceInfo, nullptr, &inFlightFences_[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create fences!");
        }
    }

    // 3. Create Semaphores (Per Swapchain Image)
    for (size_t i = 0; i < imageCount; i++) {
        if (vkCreateSemaphore(context_.getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]) !=
            VK_SUCCESS
            || vkCreateSemaphore(context_.getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]) !=
            VK_SUCCESS) {
            throw std::runtime_error("failed to create semaphores!");
        }
    }
}

void Renderer::drawFrame(VkPipeline pipeline, bool framebufferResized, const Camera &camera) {
    vkWaitForFences(context_.getDevice(), 1, &inFlightFences_[currentFrame], VK_TRUE, UINT64_MAX);
    // vkResetFences(context_.getDevice(), 1, &inFlightFences_[currentFrame]);

    uint32_t imageIndex;
    VkResult acquireNextImageResult = vkAcquireNextImageKHR(context_.getDevice(), swapChain_.getHandle(), UINT64_MAX,
                                                            imageAvailableSemaphores_[currentFrame],
                                                            VK_NULL_HANDLE, &imageIndex);

    if (acquireNextImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    }
    if (acquireNextImageResult != VK_SUCCESS && acquireNextImageResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    updateUniformBuffer(currentFrame, camera);
    // 2. NEW: If this specific image is already in use by another frame, wait for it!
    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(context_.getDevice(), 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    // Mark this image as now being used by the current frame's fence
    imagesInFlight[imageIndex] = inFlightFences_[currentFrame];

    // 3. NOW it is safe to reset the fence and continue
    vkResetFences(context_.getDevice(), 1, &inFlightFences_[currentFrame]);

    //--
    vkResetCommandBuffer(commandBuffers_[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    recordCommandBuffer(commandBuffers_[currentFrame], pipeline, imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores_[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores_[imageIndex]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(context_.getGraphicsQueue(), 1, &submitInfo, inFlightFences_[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain_.getHandle()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    VkResult queuePresentResult = vkQueuePresentKHR(context_.getPresentQueue(), &presentInfo);

    if (queuePresentResult == VK_ERROR_OUT_OF_DATE_KHR || queuePresentResult == VK_SUBOPTIMAL_KHR ||
        framebufferResized) {
        // framebufferResized = false;
        recreateSwapChain();
    } else if (queuePresentResult != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }
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
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    // --- STEP 1: Create Staging Buffer (CPU Visible) ---
    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingAllocation);

    // --- STEP 2: Map and Copy Data ---
    void *data;
    vmaMapMemory(vmaAllocator, stagingAllocation, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vmaUnmapMemory(vmaAllocator, stagingAllocation);

    // --- STEP 3: Create Vertex Buffer (GPU Local) ---
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VMA_MEMORY_USAGE_GPU_ONLY, vertexBuffer_, vertexBufferAllocation_);

    // --- STEP 4: Copy from Staging to GPU ---
    copyBuffer(stagingBuffer, vertexBuffer_, bufferSize);

    // --- STEP 5: Clean up Staging ---
    vmaDestroyBuffer(vmaAllocator, stagingBuffer, stagingAllocation);
}

void Renderer::createIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingAllocation);

    void *data;
    vmaMapMemory(vmaAllocator, stagingAllocation, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vmaUnmapMemory(vmaAllocator, stagingAllocation);

    // --- STEP 3: Create Index Buffer (GPU Local) ---
    createBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, // Use Index bit here
                 VMA_MEMORY_USAGE_GPU_ONLY,
                 indexBuffer_, indexBufferAllocation_);

    // --- STEP 4: Copy from Staging to GPU ---
    copyBuffer(stagingBuffer, indexBuffer_, bufferSize);

    // --- STEP 5: Clean up Staging ---
    vmaDestroyBuffer(vmaAllocator, stagingBuffer, stagingAllocation);
}

void Renderer::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers_.resize(engine::MAX_FRAMES_IN_FLIGHT);
    uniformBuffersAllocation_.resize(engine::MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped_.resize(engine::MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < engine::MAX_FRAMES_IN_FLIGHT; i++) {
        VmaAllocationInfo allocInfo; // This will hold our pointer

        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU, // Optimized for CPU writes
            uniformBuffers_[i],
            uniformBuffersAllocation_[i],
            VMA_ALLOCATION_CREATE_MAPPED_BIT, // Magic: Tells VMA to map it now
            &allocInfo
            );

        // Store the pointer for easy memcpy later
        uniformBuffersMapped_[i] = allocInfo.pMappedData;
    }
}

void Renderer::createBuffer(VkDeviceSize size,
                            VkBufferUsageFlags usage,
                            VmaMemoryUsage vmaUsage,
                            VkBuffer &buffer,
                            VmaAllocation &allocation,
                            VmaAllocationCreateFlags vmaFlags, // Added: for flags like MAPPED
                            VmaAllocationInfo *outAllocInfo) const {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = vmaUsage;
    allocInfo.flags = vmaFlags;

    assert(vmaAllocator != VK_NULL_HANDLE && "Allocator was never initialized!");
    assert(context_.getDevice() != VK_NULL_HANDLE && "Device handle is null!");
    // This one call replaces vkCreateBuffer, vkGetBufferMemoryRequirements,
    // , vkAllocateMemory, and vkBindBufferMemory.
    if (vmaCreateBuffer(vmaAllocator, &bufferInfo, &allocInfo, &buffer, &allocation, outAllocInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer with VMA!");
    }
}


void Renderer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool_;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(context_.getDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    {
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    }
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(context_.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(context_.getGraphicsQueue());

    vkFreeCommandBuffers(context_.getDevice(), commandPool_, 1, &commandBuffer);
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

    memcpy(uniformBuffersMapped_[currentImage], &ubo, sizeof(ubo));
}

void Renderer::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(engine::MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<uint32_t>(engine::MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(context_.getDevice(), &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void Renderer::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(engine::MAX_FRAMES_IN_FLIGHT, descriptorSetLayout_);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(engine::MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets_.resize(engine::MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(context_.getDevice(), &allocInfo, descriptorSets_.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < engine::MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers_[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets_[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(context_.getDevice(), 1, &descriptorWrite, 0, nullptr);
    }
}

void Renderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(context_.getDevice(), &layoutInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}