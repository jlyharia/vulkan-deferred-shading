//
// Created by johnny on 12/21/25.
//

#include "app.hpp"

#include <stdexcept>

#include "renderer/renderer.hpp"
#include "renderer/UserInterface.hpp"
#include "scene/Mesh.hpp"
#include "vulkan/graphics_pipeline.hpp"
#include "vulkan/swap_chain.hpp"

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
        *vulkanContext_, *swapchain_, renderer_->getDescriptorSetLayouts());

    userInterface_ = std::make_unique<UserInterface>(*vulkanContext_, *swapchain_, window_);

    renderer_->initResources();
    renderer_->setupDefaultMaterial(
        textureManager_->getWhiteFallback()->imageView,
        textureManager_->getFlatNormalFallback()->imageView,
        textureManager_->getBlackFallback()->imageView,
        textureManager_->getDefaultSampler());

    loadScene();
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
        {glm::vec3(4.0f, 3.5f, -3.0f), 8.0f, glm::vec3(1.0f, 0.45f, 0.1f), 6.0f}, // warm orange
        {glm::vec3(10.0f, 3.5f, -3.0f), 8.0f, glm::vec3(1.0f, 0.45f, 0.1f), 6.0f}, // warm orange
        {glm::vec3(6.0f, 1.0f, 2.0f), 6.0f, glm::vec3(0.4f, 0.6f, 1.0f), 8.0f},   // cool blue
        {glm::vec3(8.5f, 1.0f, 2.0f), 6.0f, glm::vec3(0.4f, 0.6f, 1.0f), 8.0f},   // cool blue
    };

    auto sphereMesh = assetManager_->getSharedSphere();

    for (size_t i = 0; i < defs.size(); ++i) {
        MeshInstance lightObj;
        lightObj.mesh  = sphereMesh;
        lightObj.name  = "PointLight_" + std::to_string(i);
        lightObj.color = glm::vec4(defs[i].color, 1.0f);
        lightObj.transform.setPosition(defs[i].position);
        lightObj.transform.setScale(glm::vec3(0.2f));
        lightObj.transform.updateMatrix();
        renderObjects_.push_back(lightObj);

        const glm::vec3 worldPos = renderObjects_.back().transform.position;
        PointLight pl{
            .position = glm::vec4(worldPos, defs[i].intensity),
            .color    = glm::vec4(defs[i].color, defs[i].radius)
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

    float frameTimeMs = deltaTime * 1000.0f;
    ImGui::Text("Frame Time: %.3f ms", frameTimeMs);
    ImGui::Text("FPS: %.1f", 1.0f / (deltaTime > 0.0f ? deltaTime : 0.001f));

    static float history[60] = {0};
    static int offset = 0;
    history[offset] = frameTimeMs;
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

    timer += deltaTime;
    if (timer >= 1.0f) {
        const float frameTimeMs = deltaTime * 1000.0f;
        const std::string title = "Vulkan Engine | " + std::to_string(frameTimeMs) + " ms";
        glfwSetWindowTitle(window_, title.c_str());
        timer = 0.0f;
    }
}

void App::processInput() {
    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window_, true);
    }
    camera.handleInput(window_, deltaTime);
}
