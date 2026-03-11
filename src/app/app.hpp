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