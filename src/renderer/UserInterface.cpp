#include "UserInterface.hpp"

#include "common/Config.hpp"
#include "scene/Camera.hpp"
#include "vulkan/SwapChain.hpp"

#include <iostream>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

UserInterface::UserInterface(VulkanContext &context, SwapChain &swapChain, GLFWwindow *window)
    : context_(context), swapChain_(swapChain) {

    createDescriptorPool();

    // 1. Setup Context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.FontGlobalScale = engineConfig::DEFAULT_GUI_FONT;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Optional: Enable Multi-Viewport (Allows windows to float outside the main window)
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    // 2. Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);

    // --- 重要：載入動態渲染函式指標 (針對 Volk/Dynamic Loader) ---
    // 這一步必須在 ImGui_ImplVulkan_Init 之前執行，否則會 Segfault
#ifndef IMGUI_IMPL_VULKAN_USE_LOADER
    // 1.92.5 的參數順序：(apiVersion, loaderFunc, userData)
    ImGui_ImplVulkan_LoadFunctions(
        engineConfig::DEFAULT_VK_API_VERSION,
        [](const char *function_name, void *user_data) {
            return vkGetInstanceProcAddr(static_cast<VkInstance>(user_data), function_name);
        },
        (void *)context_.getInstance()
        );
#endif
    // --- START OF RENDERING INFO SETUP ---
    auto colorFormat = static_cast<VkFormat>(swapChain_.getColorFormat());

    // This is the "renderingInfo" you were looking for.
    // We need to define it here so we can pass it to init_info.
    VkPipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;
    renderingInfo.depthAttachmentFormat = static_cast<VkFormat>(swapChain_.getDepthFormat());
    // --- END OF RENDERING INFO SETUP ---

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = context_.getInstance();
    init_info.PhysicalDevice = context_.getPhysicalDevice();
    init_info.Device = context_.getDevice();
    init_info.Queue = context_.getGraphicsQueue();
    init_info.DescriptorPool = imguiPool_;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;

    // 1.92.5: Move these inside PipelineInfoMain
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = renderingInfo;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        throw std::runtime_error("Failed to initialize ImGui!");
    }

    // 3. Upload Fonts (Required for 1.92.x)
    // NO NEED to call ImGui_ImplVulkan_CreateFontsTexture() here anymore!
}

UserInterface::~UserInterface() {
    std::cerr << "[Destructor] UserInterface starting..." << std::endl;
    context_.getDevice().waitIdle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (imguiPool_) {
        context_.getDevice().destroyDescriptorPool(imguiPool_);
    }
}

void UserInterface::beginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UserInterface::endFrame() {
    ImGui::Render();
}

void UserInterface::recordCommands(vk::CommandBuffer cmd, uint32_t imageIndex) const {
    // 1. Define the color attachment for the specific swapchain image
    vk::RenderingAttachmentInfo colorAttachment{};
    // Use [] to access the vector element
    colorAttachment.setImageView(swapChain_.getImageViews()[imageIndex])
                   .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                   .setLoadOp(vk::AttachmentLoadOp::eLoad) // Draw on top of scene
                   .setStoreOp(vk::AttachmentStoreOp::eStore);

    // 2. Define the rendering region
    vk::RenderingInfo renderingInfo{};
    renderingInfo.setRenderArea({{0, 0}, swapChain_.getExtent()})
                 .setLayerCount(1)
                 .setColorAttachments(colorAttachment);

    // 3. Record commands
    cmd.beginRendering(renderingInfo);

    // ImGui_ImplVulkan_RenderDrawData needs the raw C handle (VkCommandBuffer)
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VkCommandBuffer>(cmd));

    cmd.endRendering();
}

void UserInterface::createDescriptorPool() {
    // ImGui typically only needs 1 descriptor for its font texture.
    // However, if you plan to display custom images/textures in the UI later,
    // you might want to increase 'maxSets'.
    vk::DescriptorPoolSize poolSizes[] = {
        {vk::DescriptorType::eCombinedImageSampler, 1}
    };

    auto poolInfo = vk::DescriptorPoolCreateInfo()
                    .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
                    .setMaxSets(1)
                    .setPoolSizes(poolSizes);

    // This creates the pool using your device handle from the context
    imguiPool_ = context_.getDevice().createDescriptorPool(poolInfo);
}

void UserInterface::drawGpuTimings(const std::vector<GpuTimingEntry> &entries) {
    ImGui::Begin("GPU Pass Timings");
    if (ImGui::BeginTable("timings", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Pass",   ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("GPU ms", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableHeadersRow();
        float total = 0.0f;
        for (auto &e : entries) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(e.name.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f", e.gpuMs);
            total += e.gpuMs;
        }
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Total");
        ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f", total);
        ImGui::EndTable();
    }
    ImGui::End();
}

void UserInterface::drawCameraSettings(Camera &camera) {
    ImGui::Begin("Camera Controller");

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Position", &camera.position.x, 0.1f);

        // Track changes to Yaw/Pitch
        bool changed = false;
        if (ImGui::SliderFloat("Yaw", &camera.yaw, -180.0f, 180.0f))
            changed = true;
        if (ImGui::SliderFloat("Pitch", &camera.pitch, -89.0f, 89.0f))
            changed = true;

        if (changed) {
            camera.updateCameraVectors();
        }

        // --- Added Forward Vector Display ---
        ImGui::Separator();
        ImGui::Text("Forward Vector:");
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                           "X: %.3f  Y: %.3f  Z: %.3f",
                           camera.forward.x, camera.forward.y, camera.forward.z);
        // ------------------------------------
    }

    if (ImGui::CollapsingHeader("Settings")) {
        ImGui::SliderFloat("Movement Speed", &camera.movementSpeed, 0.1f, 20.0f);
        ImGui::SliderFloat("Mouse Sensitivity", &camera.mouseSensitivity, 0.01f, 1.0f);
        ImGui::SliderFloat("Field of View", &camera.fov, 30.0f, 110.0f);
    }

    if (ImGui::Button("Reset to Center")) {
        camera.position = glm::vec3(0.0f, 2.0f, 0.0f);
        camera.yaw = 0.0f;
        camera.pitch = 0.0f;
        camera.rotate(0, 0);
    }

    ImGui::End();
}