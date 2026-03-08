//
// Created by johnny on 12/21/25.
//

#pragma once
#include "common/VulkanInclude.hpp"
#include <chrono>
#include <memory>

#include "../scene/Camera.hpp"
#include "core/AssetManager.hpp"
#include "scene/RenderObject.hpp"
#include "scene/TextureManager.hpp"
#include "vulkan/VulkanContext.hpp"

class UserInterface;
class Renderer;
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
    Camera camera;

    GLFWwindow *window_ = nullptr;
    // 1. Core: Must be destroyed ABSOLUTE LAST
    std::unique_ptr<VulkanContext> vulkanContext_;

    // 2. The Foundation: Owns the windows/buffers
    std::unique_ptr<SwapChain> swapchain_;
    std::unique_ptr<Renderer> renderer_;
    // 3. Resource Management: Owns the actual GPU Memory (VMA)
    // Must die before Context, but AFTER the objects that use the memory
    std::unique_ptr<GraphicsPipeline> graphicsPipeline_;
    std::unique_ptr<TextureManager> textureManager_;
    std::unique_ptr<AssetManager> assetManager_;

    // 4. Descriptor & Command Logic: Owns the Pools
    // If this dies before the models, the models can't free their DescriptorSets!


    // 5. The State: Depends on DescriptorSetLayouts (from Renderer) and Swapchain


    // 6. High Level objects: Depends on everything (Destroyed FIRST)
    std::unique_ptr<UserInterface> userInterface_;
    std::shared_ptr<Model> sponzaModel_;
    std::vector<RenderObject> renderObjects_;

    void initWindow();

    void mainLoop();

    void initVulkan();

    void drawFrame();

    bool framebufferResized = false;

    static void framebufferResizeCallback(GLFWwindow *window, int width, int height);

    void updateFrameTime(); // Our new extracted function
    void loadScene();
    // State variables for time tracking
    std::chrono::high_resolution_clock::time_point lastTime;
    float timer = 0.0f;
    float deltaTime = 0.0f;

    bool framebufferResized_ = false;

    std::chrono::time_point<std::chrono::high_resolution_clock> lastFrameTime;

    void renderUI() const;
    void processInput();
    void update(float dt);


};