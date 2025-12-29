//
// Created by johnny on 12/21/25.
//

#pragma once
#include <GLFW/glfw3.h>
#include <memory>

#include "vulkan/VulkanContext.hpp"

class RenderPass;
class GraphicsPipeline;
class SwapChain;

class App {
public:
    App(int width, int height, const char *title);

    // ============================================================
    // Vulkan Resource Destruction Order (Comments Only)
    // ============================================================
    //
    // 0. Make sure GPU is no longer using any resources
    //    - Wait for all queues to finish execution
    //
    // 1. Swapchain-dependent resources (destroy first)
    //    - Framebuffers (depend on image views + render pass)
    //    - Swapchain image views (wrap swapchain images)
    //    - Swapchain (presentation engine connection)
    //
    // 2. Graphics pipelines
    //    - Graphics pipelines (reference pipeline layout + render pass)
    //
    // 3. Pipeline layouts
    //    - Pipeline layouts (reference descriptor set layouts + push constants)
    //
    // 4. Descriptor-related resources
    //    - Descriptor pools (implicitly free descriptor sets)
    //    - Descriptor set layouts (describe resource bindings)
    //
    // 5. Render pass (classic render pass path only)
    //    - Skip if using dynamic rendering
    //
    // 6. Command resources
    //    - Command pool (implicitly frees command buffers)
    //
    // 7. Logical device
    //    - Destroys all remaining device-level resources
    //
    // 8. Instance-level resources
    //    - Surface (window-system integration)
    //    - Debug messenger (validation layers)
    //    - Vulkan instance
    //
    // ============================================================
    // Rule of thumb:
    // Destroy in reverse order of creation and dependency.
    // If A uses B, destroy A before B.
    // ============================================================
    ~App();

    void run();

private:
    // Basic Data
    int width_;
    int height_;
    const char *title_;

    // 1. GLFW Window (Destroyed LAST)
    GLFWwindow *window_ = nullptr;

    // 2. Vulkan Context / Device (Destroyed 2nd to last)
    std::unique_ptr<VulkanContext> vulkanContext_;

    // 3. Render Pass (The "Contract")
    // Needs to be declared BEFORE the pipeline and swapchain/framebuffers
    std::unique_ptr<RenderPass> renderPass_;;

    // 4. Swapchain (Owns Images/Views/Framebuffers)
    std::unique_ptr<SwapChain> swapchain_;

    // 5. Pipeline (Destroyed FIRST)
    // Depends on the Render Pass
    std::unique_ptr<GraphicsPipeline> graphicsPipeline_;

    void initWindow();

    void mainLoop() const;

    void initVulkan();
};
