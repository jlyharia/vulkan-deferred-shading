//
// Created by johnny on 12/21/25.
//

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "app.hpp"

#include <stdexcept>

#include "renderer/renderer.hpp"
#include "vulkan/graphics_pipeline.hpp"
#include "vulkan/render_pass.hpp"
#include "vulkan/swap_chain.hpp"

App::App(int width, int height, const char *title)
    : width_(width), height_(height), title_(title) {
}

App::~App() {
    // Wait for GPU to be idle before destroying anything
    if (vulkanContext_)
        vkDeviceWaitIdle(vulkanContext_->getDevice());

    renderer_.reset(); // 5. Destroys sync objects/cmd buffers
    graphicsPipeline_.reset(); // 4. Destroys pipeline
    renderPass_.reset(); // 3. Destroys render pass
    swapchain_.reset(); // 2. Destroys framebuffers/image views
    vulkanContext_.reset(); // 1. Finally, destroys Device and Instance

    if (window_)
        glfwDestroyWindow(window_);
    glfwTerminate();
}

void App::initWindow() {
    if (!glfwInit())
        throw std::runtime_error("Failed to init GLFW");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(width_, height_, title_, nullptr, nullptr);
    if (!window_)
        throw std::runtime_error("Failed to create GLFW window");
    // 2. CRITICAL: Link this C++ object instance to the GLFW window
    glfwSetWindowUserPointer(window_, this);

    // 3. Set the callback
    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
}

void App::framebufferResizeCallback(GLFWwindow *window, int width, int height) {
    auto app = reinterpret_cast<App *>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}

void App::initVulkan() {
    vulkanContext_ = std::make_unique<VulkanContext>(window_, true);
    swapchain_ = std::make_unique<SwapChain>(*vulkanContext_, window_);
    renderPass_ = std::make_unique<RenderPass>(*vulkanContext_,
                                               swapchain_->getColorFormat(),
                                               swapchain_->getDepthFormat());
    swapchain_->createFramebuffers(renderPass_->getRenderPass());

    // --- NEW PROFESSIONAL SEQUENCE ---

    // 1. Create Renderer (Minimal state)
    renderer_ = std::make_unique<Renderer>(*vulkanContext_, *swapchain_, *renderPass_, window_);

    // 2. Create the Layout (The Blueprint)
    renderer_->createDescriptorSetLayout();

    // 3. Create Pipeline (The Logic) - Pass the layout FROM the renderer
    graphicsPipeline_ = std::make_unique<GraphicsPipeline>(
        *vulkanContext_,
        *swapchain_,
        renderPass_->getRenderPass(),
        renderer_->getDescriptorSetLayout() // <--- This is the key link
        );

    // 4. Initialize Renderer Resources (The Data)
    // Pass the pipeline layout so the Renderer knows how to bind sets
    renderer_->initResources(graphicsPipeline_->getPipelineLayout(), "assets/model/sphere_grid.obj");
}

void App::mainLoop() {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        processInput();
        // Keep the logic separate from the drawing
        updateFrameTime();
        drawFrame();
    }

    // Wait for GPU to finish before exiting to avoid crashing during cleanup
    vkDeviceWaitIdle(vulkanContext_->getDevice());
}

void App::drawFrame() {
    // We check the flag here, or inside renderer_->drawFrame()
    // For a Senior architecture, the Renderer should report if it needs a resize
    try {
        renderer_->drawFrame(graphicsPipeline_->getPipeline(), framebufferResized_, camera);
    } catch (const std::runtime_error &e) {
        // If the renderer encounters VK_ERROR_OUT_OF_DATE_KHR, it throws
        renderer_->recreateSwapChain();
    }

    if (framebufferResized_) {
        renderer_->recreateSwapChain();
        framebufferResized_ = false;
    }
}

void App::run() {
    initWindow();
    initVulkan();
    mainLoop();
}


void App::updateFrameTime() {
    // 1. Calculate delta time
    const auto currentTime = std::chrono::high_resolution_clock::now();

    // We calculate the difference between 'now' and 'lastTime'
    deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
    lastTime = currentTime;

    // 2. Handle printing (Aggregated to 1 second intervals)
    timer += deltaTime;
    if (timer >= 1.0f) {
        const float frameTimeMs = deltaTime * 1000.0f;
        // std::cout << "Frame Time: " << frameTimeMs << "ms" << std::endl;

        // Use the window title trick for a cleaner console
        const std::string title = "Vulkan Engine | " + std::to_string(frameTimeMs) + " ms";
        glfwSetWindowTitle(window_, title.c_str());

        timer = 0.0f;
    }
}


void App::processInput() {
    // 1. Calculate DeltaTime
    auto currentTime = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(currentTime - lastFrameTime).count();
    lastFrameTime = currentTime;
    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window_, true);
    }
    camera.handleInput(window_, dt);
}