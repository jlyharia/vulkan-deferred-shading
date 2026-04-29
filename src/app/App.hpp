//
// Created by johnny on 12/21/25.
//

#pragma once
#include "common/VulkanInclude.hpp"
#include "renderer/UserInterface.hpp"
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "../scene/Camera.hpp"
#include "assets/AssetManager.hpp"
#include "scene/DirLightView.hpp"
#include "scene/MeshInstance.hpp"
#include "scene/TextureManager.hpp"
#include "scene/PointLight.hpp"
#include "vulkan/VulkanContext.hpp"

// UserInterface included above (needed for GpuTimingEntry)
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
    std::vector<MeshInstance> renderObjects_;

    // Point light data — positions derived from sphere MeshInstance transforms
    std::vector<PointLight> pointLights_;
    DirLightView dirLight_;

    void initWindow();

    void mainLoop();

    void initVulkan();

    void drawFrame();

    static void framebufferResizeCallback(GLFWwindow *window, int width, int height);

    void updateFrameTime();
    void accumulateGpuTimings();
    void loadScene();
    void loadPointLights();

    // State variables for time tracking
    std::chrono::high_resolution_clock::time_point lastTime;
    float timer            = 0.0f;
    float deltaTime        = 0.0f;
    float frameTimeAccum_  = 0.0f;
    int   frameCount_      = 0;
    float avgFrameTimeMs_  = 0.0f;

    // GPU pass timing averages (1-second flush, mirrors CPU frame time pattern)
    std::vector<float>                           gpuTimingAccum_;
    std::vector<std::string>                     gpuTimingNames_;
    int                                          gpuTimingCount_ = 0;
    float                                        gpuTimer_       = 0.0f;
    std::vector<UserInterface::GpuTimingEntry>   avgGpuTimings_;

    bool framebufferResized_ = false;

    void renderUI() const;
    void processInput();
    void update(float dt);
};
