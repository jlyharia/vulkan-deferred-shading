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
    textureManager_ = std::make_unique<TextureManager>(*vulkanContext_);
    userInterface_ = std::make_unique<UserInterface>(*vulkanContext_, *swapchain_, window_);

    renderer_->initResources();

    assetManager_ = std::make_unique<AssetManager>(*vulkanContext_, vulkanContext_->getMainCommandPool(),
                                                   *textureManager_,
                                                   renderer_->getDescriptorPool(),
                                                   renderer_->getTextureDescriptorSetLayout());

    graphicsPipeline_ = std::make_unique<GraphicsPipeline>(
        *vulkanContext_, *swapchain_,
        renderer_->getDescriptorSetLayouts(),
        renderer_->getSsaoBlurLayout());
    renderer_->setupDefaultMaterial(
        textureManager_->getWhiteFallback()->imageView,
        textureManager_->getFlatNormalFallback()->imageView,
        textureManager_->getBlackFallback()->imageView,
        textureManager_->getDefaultSampler());

    loadScene();
    renderer_->setSphereMesh(assetManager_->getSharedSphere());
}

void App::loadScene() {
    if (useCsmDebugScene_) {
        loadCsmDebugScene();
        return;
    }

    loadSponzaScene();

    if (useMultipleSphereScene_) {
        loadMultipleSphereScene();
    }

    loadPointLights();
}

void App::loadSponzaScene() {
    auto sponzaMesh = assetManager_->loadModel("assets/model/sponza_palace/scene.gltf");

    MeshInstance sponzaInstance;
    sponzaInstance.mesh = sponzaMesh;
    sponzaInstance.name = "Sponza_Main_Building";
    sponzaInstance.transform.setScale(glm::vec3(1.0f));
    sponzaInstance.transform.setRotation(glm::vec3(90.0f, 0.0f, 0.0f));
    sponzaInstance.transform.setPosition(glm::vec3(7.0f, 1.5f, -4.0f));
    sponzaInstance.transform.updateMatrix();
    renderObjects_.push_back(sponzaInstance);
}
void App::loadMultipleSphereScene() {
    auto sphereMesh = assetManager_->loadModel("assets/model/basic/sphere.glb");

    constexpr int gridSize = 12;
    constexpr float spacing = 3.5f;
    for (int x = 0; x < gridSize; ++x) {
        for (int y = 0; y < gridSize; ++y) {
            MeshInstance obj;
            obj.mesh = sphereMesh;
            obj.name = "FrustumDebug_" + std::to_string(x) + "_" + std::to_string(y);
            obj.transform.setPosition(glm::vec3(
                (x - gridSize / 2) * spacing + 7.0f,
                (y - gridSize / 2) * spacing + 1.5f,
                0.5f));
            obj.transform.setScale(glm::vec3(0.03f));
            obj.transform.updateMatrix();
            renderObjects_.push_back(obj);
        }
    }
}
void App::loadCsmDebugScene() {
    // Light from behind/above the default camera (which looks +X from origin),
    // so the cylinder fronts face toward the light and show curvature shading.
    dirLight_.position = glm::vec3(-20.0f, -20.0f, 50.0f);
    dirLight_.target   = glm::vec3(50.0f,  0.0f,  -15.0f);

    auto floorMesh    = assetManager_->loadModel("assets/model/floor/checkered_tile_floor.glb");
    auto cylinderMesh = assetManager_->loadModel("assets/model/cylinder/cylinder.glb");

    MeshInstance floor;
    floor.mesh = floorMesh;
    floor.name = "CsmDebug_Floor";
    floor.transform.setScale(glm::vec3(1.0f, 1.0f, 1.0f));
    floor.transform.setRotation(glm::vec3(90.0f, 0.0f, 0.0f));
    floor.transform.setPosition(glm::vec3(40.0f, 0.0f, -20.0f));
    floor.transform.updateMatrix();
    renderObjects_.push_back(floor);

    // Cylinders straddling each cascade split (Z-up world: lined up along +Y)
    // Cascade far planes ~5 / 25 / 80 / 200 m with shadowFar=200, lambda=0.9
    const float xPos[4] = { -10.0f, 15.0f, 50.0f, 130.0f };
    for (int i = 0; i < 4; ++i) {
        MeshInstance cyl;
        cyl.mesh = cylinderMesh;
        cyl.name = "CsmDebug_Cylinder_" + std::to_string(i);
        cyl.transform.setRotation(glm::vec3(90.0f, 0.0f, 0.0f));
        cyl.transform.setPosition(glm::vec3(xPos[i], 0.0f , -10.0f));
        cyl.transform.setScale(glm::vec3(0.05f, 0.5f, 0.05f));

        cyl.transform.updateMatrix();
        renderObjects_.push_back(cyl);
    }
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
        accumulateGpuTimings();
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

    userInterface_->drawGpuTimings(avgGpuTimings_);
}

void App::drawFrame() {
    try {
        renderer_->drawFrame(*graphicsPipeline_,
                             framebufferResized_,
                             camera,
                             *userInterface_,
                             renderObjects_,
                             pointLights_,
                             dirLight_);
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

void App::accumulateGpuTimings() {
    const auto &timings = renderer_->gpuTimings();
    if (timings.empty()) return;

    if (gpuTimingAccum_.size() != timings.size()) {
        gpuTimingAccum_.assign(timings.size(), 0.0f);
        gpuTimingNames_.clear();
        for (auto &e : timings) gpuTimingNames_.push_back(e.name);
    }
    for (size_t i = 0; i < timings.size(); ++i)
        gpuTimingAccum_[i] += timings[i].gpuMs;
    gpuTimingCount_++;
    gpuTimer_ += deltaTime;

    if (gpuTimer_ >= 1.0f && gpuTimingCount_ > 0) {
        avgGpuTimings_.clear();
        for (size_t i = 0; i < gpuTimingAccum_.size(); ++i)
            avgGpuTimings_.push_back({gpuTimingNames_[i], gpuTimingAccum_[i] / gpuTimingCount_});
        gpuTimingAccum_.assign(gpuTimingAccum_.size(), 0.0f);
        gpuTimingCount_ = 0;
        gpuTimer_       = 0.0f;
    }
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