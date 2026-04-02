//
// Created by johnny on 12/21/25.
//

#include "App.hpp"

#include <stdexcept>

#include "renderer/Renderer.hpp"
#include "renderer/UserInterface.hpp"
#include "scene/Mesh.hpp"
#include "vulkan/GraphicsPipeline.hpp"
#include "vulkan/SwapChain.hpp"

#include <imgui.h>

App::App(int width, int height, const char *title) : width_(width), height_(height), title_(title) {
}

App::~App() {
    if (vulkanContext_ && vulkanContext_->getDevice()) {
        vulkanContext_->getDevice().waitIdle();
    }

    if (window_) {
        glfwDestroyWindow(window_);
    }
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
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
}

void App::framebufferResizeCallback(GLFWwindow *window, int width, int height) {
    auto app = reinterpret_cast<App *>(glfwGetWindowUserPointer(window));
    app->framebufferResized_ = true;
}

void App::initVulkan() {
    vulkanContext_ = std::make_unique<VulkanContext>(window_, true);
    swapchain_ = std::make_unique<SwapChain>(*vulkanContext_, window_);

    renderer_ = std::make_unique<Renderer>(*vulkanContext_, *swapchain_, window_);
    renderer_->createDescriptorSetLayout();

    textureManager_ = std::make_unique<TextureManager>(*vulkanContext_);
    assetManager_ = std::make_unique<AssetManager>(*vulkanContext_, vulkanContext_->getMainCommandPool(),
                                                   *textureManager_, *renderer_);

    graphicsPipeline_ = std::make_unique<GraphicsPipeline>(
        *vulkanContext_, *swapchain_,
        renderer_->getDescriptorSetLayouts(),
        renderer_->getSsaoBlurLayout());

    userInterface_ = std::make_unique<UserInterface>(*vulkanContext_, *swapchain_, window_);

    renderer_->initResources();
    renderer_->setupDefaultMaterial(
        textureManager_->getWhiteFallback()->imageView,
        textureManager_->getFlatNormalFallback()->imageView,
        textureManager_->getBlackFallback()->imageView,
        textureManager_->getDefaultSampler());

    loadScene();
    renderer_->setSphereMesh(assetManager_->getSharedSphere());
}

void App::loadScene() {
    auto sponzaMesh = assetManager_->loadModel("assets/model/sponza_palace/scene.gltf");

    MeshInstance sponzaInstance;
    sponzaInstance.mesh = sponzaMesh;
    sponzaInstance.name = "Sponza_Main_Building";
    sponzaInstance.transform.setScale(glm::vec3(1.0f));
    sponzaInstance.transform.setRotation(glm::vec3(90.0f, 0.0f, 0.0f));
    sponzaInstance.transform.setPosition(glm::vec3(7.0f, 1.5f, -4.0f));
    sponzaInstance.transform.updateMatrix();
    renderObjects_.push_back(sponzaInstance);

    loadPointLights();
}

void App::loadPointLights() {
    struct LightDef {
        glm::vec3 position;
        float intensity;
        glm::vec3 color;
        float radius;
    };

    const std::vector<LightDef> defs = {
        // warm orange (upper, far)
        {glm::vec3(4.0f, 3.5f, -3.0f),  4.0f, glm::vec3(1.0f, 0.45f, 0.1f), 6.0f},
        {glm::vec3(10.0f, 3.5f, -3.0f), 4.0f, glm::vec3(1.0f, 0.45f, 0.1f), 6.0f},
        // cool blue (lower, near)
        {glm::vec3(6.0f, 1.0f, 2.0f),   3.0f, glm::vec3(0.4f, 0.6f, 1.0f),  8.0f},
        {glm::vec3(8.5f, 1.0f, 2.0f),   3.0f, glm::vec3(0.4f, 0.6f, 1.0f),  8.0f},
        // warm white (upper, mid)
        {glm::vec3(2.0f, 4.0f, -1.0f),  3.5f, glm::vec3(1.0f, 0.95f, 0.8f), 6.0f},
        {glm::vec3(7.0f, 4.0f, -1.0f),  3.5f, glm::vec3(1.0f, 0.95f, 0.8f), 6.0f},
        {glm::vec3(12.0f, 4.0f, -1.0f), 3.5f, glm::vec3(1.0f, 0.95f, 0.8f), 6.0f},
        // green accent (ground level)
        {glm::vec3(3.5f, 0.5f, -2.0f),  2.5f, glm::vec3(0.3f, 1.0f, 0.4f),  5.0f},
        {glm::vec3(10.5f, 0.5f, -2.0f), 2.5f, glm::vec3(0.3f, 1.0f, 0.4f),  5.0f},
        // purple (high, flanks)
        {glm::vec3(1.0f, 5.0f, -3.5f),  3.0f, glm::vec3(0.7f, 0.3f, 1.0f),  7.0f},
        {glm::vec3(13.0f, 5.0f, -3.5f), 3.0f, glm::vec3(0.7f, 0.3f, 1.0f),  7.0f},
        // red torch (low, corners)
        {glm::vec3(2.0f, 1.5f, 1.5f),   2.5f, glm::vec3(1.0f, 0.2f, 0.1f),  4.0f},
        {glm::vec3(12.0f, 1.5f, 1.5f),  2.5f, glm::vec3(1.0f, 0.2f, 0.1f),  4.0f},
        // cyan fill (mid height)
        {glm::vec3(5.0f, 2.5f, -3.5f),  3.0f, glm::vec3(0.2f, 0.9f, 0.9f),  7.0f},
        {glm::vec3(9.0f, 2.5f, -3.5f),  3.0f, glm::vec3(0.2f, 0.9f, 0.9f),  7.0f},
        // neutral white (center, high)
        {glm::vec3(7.0f, 6.0f, -1.5f),  4.5f, glm::vec3(1.0f, 1.0f, 1.0f),  8.0f},
        // right flank (X 12.4–15.5)
        {glm::vec3(12.4f, 2.0f, -2.0f),  3.0f, glm::vec3(1.0f, 0.45f, 0.1f), 5.0f}, // warm orange
        {glm::vec3(13.8f, 1.0f, 0.5f),   2.5f, glm::vec3(0.4f, 0.6f, 1.0f),  6.0f}, // cool blue
        {glm::vec3(15.0f, 3.0f, -3.5f),  3.0f, glm::vec3(0.7f, 0.3f, 1.0f),  7.0f}, // purple
        {glm::vec3(15.5f, 0.5f, -1.0f),  2.5f, glm::vec3(0.3f, 1.0f, 0.4f),  5.0f}, // green
        // underground / below floor
        {glm::vec3(4.0f, -0.5f, -1.5f),  2.5f, glm::vec3(0.9f, 0.7f, 0.3f),  5.0f}, // amber
        {glm::vec3(10.0f, -0.8f, -1.5f), 2.5f, glm::vec3(0.9f, 0.7f, 0.3f),  5.0f}, // amber
        {glm::vec3(6.5f, -1.6f, -0.5f),  2.0f, glm::vec3(0.5f, 0.7f, 1.0f),  6.0f}, // cool blue
        {glm::vec3(7.5f, -2.3f, -2.5f),  2.0f, glm::vec3(0.5f, 0.7f, 1.0f),  6.0f}, // cool blue
    };

    auto sphereMesh = assetManager_->getSharedSphere();

    for (size_t i = 0; i < defs.size(); ++i) {
        MeshInstance lightObj;
        lightObj.mesh = sphereMesh;
        lightObj.name = "PointLight_" + std::to_string(i);
        lightObj.color = glm::vec4(defs[i].color, 1.0f);
        lightObj.transform.setPosition(defs[i].position);
        lightObj.transform.setScale(glm::vec3(0.05f));
        lightObj.transform.updateMatrix();
        renderObjects_.push_back(lightObj);

        const glm::vec3 worldPos = renderObjects_.back().transform.position;
        PointLight pl{
            .position = glm::vec4(worldPos, defs[i].intensity),
            .color = glm::vec4(defs[i].color, defs[i].radius)
        };
        pointLights_.push_back(pl);
    }
}

void App::mainLoop() {
    lastTime = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        updateFrameTime();
        processInput();
        update(deltaTime);

        userInterface_->beginFrame();
        renderUI();
        userInterface_->drawCameraSettings(camera);
        userInterface_->endFrame();

        drawFrame();
    }
    vkDeviceWaitIdle(vulkanContext_->getDevice());
}

void App::update(float dt) {
    for (auto &obj : renderObjects_) {
        obj.transform.updateMatrix();
    }
}

void App::renderUI() const {
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), dockspace_flags);
    ImGui::Begin("Performance Monitor");

    ImGui::Text("Frame Time: %.3f ms", avgFrameTimeMs_);
    ImGui::Text("FPS: %.1f", avgFrameTimeMs_ > 0.0f ? 1000.0f / avgFrameTimeMs_ : 0.0f);

    static float history[60] = {0};
    static int offset = 0;
    history[offset] = avgFrameTimeMs_;
    offset = (offset + 1) % 60;
    ImGui::PlotLines("Latency", history, 60, offset, nullptr, 0.0f, 33.3f, ImVec2(0, 50));

    ImGui::End();
}

void App::drawFrame() {
    try {
        renderer_->drawFrame(*graphicsPipeline_,
                             framebufferResized_,
                             camera,
                             *userInterface_,
                             renderObjects_,
                             pointLights_);
    } catch (const std::runtime_error &e) {
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
    const auto currentTime = std::chrono::high_resolution_clock::now();
    deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
    lastTime = currentTime;

    frameTimeAccum_ += deltaTime;
    frameCount_++;

    timer += deltaTime;
    if (timer >= 1.0f) {
        avgFrameTimeMs_ = (frameTimeAccum_ / frameCount_) * 1000.0f;
        const std::string title = "Vulkan Engine | " + std::to_string(avgFrameTimeMs_) + " ms";
        glfwSetWindowTitle(window_, title.c_str());
        frameTimeAccum_ = 0.0f;
        frameCount_     = 0;
        timer           = 0.0f;
    }
}

void App::processInput() {
    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window_, true);
    }
    camera.handleInput(window_, deltaTime);
}