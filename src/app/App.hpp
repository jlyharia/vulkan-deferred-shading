//
// Created by johnny on 12/21/25.
//

#pragma once

#include <GLFW/glfw3.h>
#include "vulkan/VulkanContext.hpp"

class App {
public:
    App() = default;
    ~App() = default;

    void run();

private:
    void initWindow();
    void mainLoop();
    void cleanup();

    GLFWwindow* window_ = nullptr;
    VulkanContext* vulkanContext_ = nullptr;

    const int WIDTH = 800;
    const int HEIGHT = 600;
};

// vulkan clean up order -- Pipelines → Framebuffers → Swapchain → Device → Instance → Window
