//
// Created by johnny on 12/29/25.
//

#include "renderer.hpp"

#include "common/config.hpp"
#include "vulkan/graphics_pipeline.hpp"
#include "vulkan/render_pass.hpp"
#include "vulkan/swap_chain.hpp"
#include "vulkan/VulkanContext.hpp"

/**
* In Vulkan, the specific order of destruction between a Semaphore and a Command Pool does not technically matter, as long as they are both destroyed after the GPU has finished using them.
*
* However, there is a "Logical Best Practice" that most engine developers follow to keep code clean and mirror the creation order.
 */
Renderer::~Renderer() {
    // 1. Ensure GPU is idle before we start deleting things
    vkDeviceWaitIdle(context_.getDevice());

    // 2. Destroy Fences (Per Frame Slot)
    for (const auto &fence: inFlightFences_) {
        vkDestroyFence(context_.getDevice(), fence, nullptr);
    }

    // 3. Destroy Semaphores (Per Swapchain Image)
    for (const auto &semaphore: imageAvailableSemaphores_) {
        vkDestroySemaphore(context_.getDevice(), semaphore, nullptr);
    }

    for (const auto &semaphore: renderFinishedSemaphores_) {
        vkDestroySemaphore(context_.getDevice(), semaphore, nullptr);
    }

    // 4. Destroy Command Pool (Implicitly frees all Command Buffers)
    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(context_.getDevice(), commandPool_, nullptr);
    }
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

void Renderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
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

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    // begin render pass
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.getPipeline());

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChain_.getExtent().width;
        viewport.height = (float) swapChain_.getExtent().height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChain_.getExtent();
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void Renderer::createSyncObjects() {
    const uint32_t imageCount = static_cast<uint32_t>(swapChain_.getImages().size());

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

void Renderer::drawFrame() {
    vkWaitForFences(context_.getDevice(), 1, &inFlightFences_[currentFrame], VK_TRUE, UINT64_MAX);
    // vkResetFences(context_.getDevice(), 1, &inFlightFences_[currentFrame]);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(context_.getDevice(), swapChain_.getHandle(), UINT64_MAX,
                          imageAvailableSemaphores_[currentFrame],
                          VK_NULL_HANDLE, &imageIndex);

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
    recordCommandBuffer(commandBuffers_[currentFrame], imageIndex);

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

    vkQueuePresentKHR(context_.getPresentQueue(), &presentInfo);
    currentFrame = (currentFrame + 1) % engine::MAX_FRAMES_IN_FLIGHT;
}
