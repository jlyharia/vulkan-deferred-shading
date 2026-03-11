//
// Created by johnny on 12/21/25.
//

#include "app.hpp"

#include <stdexcept>

#include "renderer/renderer.hpp"
#include "renderer/UserInterface.hpp"
#include "scene/Model.hpp"
#include "vulkan/graphics_pipeline.hpp"
#include "vulkan/swap_chain.hpp"

#include <imgui.h>

App::App(int width, int height, const char *title) : width_(width), height_(height), title_(title) {
}

App::~App() {
    if (vulkanContext_ && vulkanContext_->getDevice()) {
        vulkanContext_->getDevice().waitIdle();
    }

    // You don't NEED to call .reset() on everything anymore.
    // C++ will now destroy them in the correct order automatically:
    // 1. UI, RenderObjects, SponzaModel (Releases references)
    // 2. Asset/Texture Managers (Releases VMA memory)
    // 3. GraphicsPipeline
    // 4. Renderer (Releases Pools/Layouts)
    // 5. Swapchain
    // 6. VulkanContext (Destroy Device/Instance)

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

    // 1. Create Renderer first to get the Command Pool
    renderer_ = std::make_unique<Renderer>(*vulkanContext_, *swapchain_, window_);
    renderer_->createDescriptorSetLayout();

    textureManager_ = std::make_unique<TextureManager>(*vulkanContext_);
    // 2. Create AssetManager using the pool from the renderer
    assetManager_ = std::make_unique<AssetManager>(*vulkanContext_, vulkanContext_->getMainCommandPool(),
                                                   *textureManager_, *renderer_);

    // 3. Create Pipeline using the Layout from the Renderer
    graphicsPipeline_ = std::make_unique<GraphicsPipeline>(
        *vulkanContext_, *swapchain_, renderer_->getDescriptorSetLayouts()
        );

    userInterface_ = std::make_unique<UserInterface>(*vulkanContext_, *swapchain_, window_);

    // 4. Initialize Renderer internal buffers/sets with the Pipeline Layout
    renderer_->initResources();
    renderer_->setupDefaultMaterial(
        textureManager_->getWhiteFallback().imageView,
        textureManager_->getFlatNormalFallback().imageView,
        textureManager_->getBlackFallback().imageView,
        textureManager_->getDefaultSampler()
        );

    // 5. Load your objects (calls assetManager->loadModel internally)
    loadScene();
}

void App::loadScene() {
    // 1. Load the Sponza Mesh (Geometry)
    sponzaModel_ = std::make_shared<Model>(vulkanContext_->getVmaAllocator(), vulkanContext_->getDevice());
    sponzaModel_->loadFromFile(
        // "assets/model/basic/sphere.glb",
        // "assets/model/sponza/glTF/Sponza.gltf",
        "assets/model/sponza_palace/scene.gltf",
        *textureManager_, *renderer_
        );

    // Upload geometry to GPU memory
    sponzaModel_->uploadToGPU(vulkanContext_->getDevice(), vulkanContext_->getGraphicsQueue(),
                              vulkanContext_->getMainCommandPool());

    // 2. Create the RenderObject (The Instance)
    RenderObject sponzaInstance;
    sponzaInstance.model = sponzaModel_;
    sponzaInstance.name = "Sponza_Main_Building";

    // Sponza is often modeled in centimeters; you might need to scale it down
    sponzaInstance.transform.setScale(glm::vec3(1.0f));
    // float rot = 0.1f;
    sponzaInstance.transform.setRotation(glm::vec3(90.0f, 0.0f, 0.0f));
    sponzaInstance.transform.setPosition(glm::vec3(7.0f, 1.5f, -4.0f));
    sponzaInstance.transform.updateMatrix();

    // 3. Add to the render list
    renderObjects_.push_back(sponzaInstance);
}

void App::mainLoop() {

    // Initialize lastTime right before starting the loop to avoid a massive
    // first-frame jump
    lastTime = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        // 1. Logic Phase
        updateFrameTime();
        processInput(); // Internally updates camera
        update(deltaTime); // Internally updates all object matrices

        // 2. UI Phase
        userInterface_->beginFrame();
        renderUI(); // Call our extracted function
        userInterface_->drawCameraSettings(camera);
        userInterface_->endFrame();

        // 3. Render Phase
        drawFrame();
    }
    vkDeviceWaitIdle(vulkanContext_->getDevice());
}

void App::update(float dt) {
    // 1. Process Input and Move Camera
    // It's cleaner to handle camera input inside the update loop
    // where 'dt' is already calculated.
    // camera.handleInput(window_, dt);

    // 3. Update all GameObjects (Sponza parts, etc.)
    for (auto &obj : renderObjects_) {
        obj.transform.updateMatrix();
    }
}

void App::renderUI() const {
    // ---------------------------------------------------------
    // UI DEFINITION SECTION
    // ---------------------------------------------------------
    // This creates an invisible "docking zone" over your whole window.
    // Your other windows (Stats, Settings) can now be snapped to the edges.
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), dockspace_flags);
    ImGui::Begin("Performance Monitor");

    float frameTimeMs = deltaTime * 1000.0f;
    ImGui::Text("Frame Time: %.3f ms", frameTimeMs);
    ImGui::Text("FPS: %.1f", 1.0f / (deltaTime > 0.0f ? deltaTime : 0.001f));

    // Simple visualizer for frame stability
    static float history[60] = {0};
    static int offset = 0;
    history[offset] = frameTimeMs;
    offset = (offset + 1) % 60;
    ImGui::PlotLines("Latency", history, 60, offset, nullptr, 0.0f, 33.3f, ImVec2(0, 50));

    ImGui::End();
}

void App::drawFrame() {
    // We check the flag here, or inside renderer_->drawFrame()
    // For a Senior architecture, the Renderer should report if it needs a resize
    try {
        renderer_->drawFrame(graphicsPipeline_->getPipeline(),
                             framebufferResized_,
                             camera,
                             *userInterface_,
                             renderObjects_,
                             graphicsPipeline_->getPipelineLayout());
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
    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window_, true);
    }

    // Use the passed in dt!
    camera.handleInput(window_, deltaTime);
}