//
// Created by johnny on 12/21/25.
//

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "app.hpp"

#include <stdexcept>

#include "vulkan/graphics_pipeline.hpp"
#include "vulkan/render_pass.hpp"
#include "vulkan/swap_chain.hpp"

App::App(int width, int height, const char *title)
    : width_(width), height_(height), title_(title) {
}

App::~App() {
    graphicsPipeline_.reset();
    renderPass_.reset();
    swapchain_.reset();
    vulkanContext_.reset(); // Vulkan RAII cleanup, // calls ~VulkanContext()
    if (window_) glfwDestroyWindow(window_);
    glfwTerminate();
}

void App::initWindow() {
    if (!glfwInit()) throw std::runtime_error("Failed to init GLFW");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window_ = glfwCreateWindow(width_, height_, title_, nullptr, nullptr);
    if (!window_) throw std::runtime_error("Failed to create GLFW window");
}

void App::initVulkan() {
    // Create Vulkan context after window is ready
    vulkanContext_ = std::make_unique<VulkanContext>(window_, true);

    // Step 1: Create Swapchain to get the Format and Images
    swapchain_ = std::make_unique<SwapChain>(*vulkanContext_, window_);

    // Step 2: Create RenderPass using that Format
    renderPass_ = std::make_unique<RenderPass>(*vulkanContext_, swapchain_->getSwapChainImageFormat());

    // Step 3: INVOKE HERE
    // Now that both the Images (in swapchain) and the Blueprint (renderpass) exist,
    // we can link them together into Framebuffers.
    swapchain_->createFramebuffers(renderPass_->getRenderPass());

    // Step 4: Create Pipeline
    graphicsPipeline_ = std::make_unique<GraphicsPipeline>(*vulkanContext_, *swapchain_, renderPass_->getRenderPass());
}

void App::mainLoop() const {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
    }
}

void App::run() {
    initWindow();
    initVulkan();
    mainLoop();
}
