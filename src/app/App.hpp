//
// Created by johnny on 12/21/25.
//

#pragma once
#include <GLFW/glfw3.h>
#include <memory>

#include "vulkan/VulkanContext.hpp"

class Swapchain;

class App {
public:
    App(int width, int height, const char *title);

    ~App();

    void run();

private:
    int width_;
    int height_;
    const char *title_;

    GLFWwindow *window_ = nullptr;
    std::unique_ptr<VulkanContext> vulkanContext_;
    std::unique_ptr<Swapchain> swapchain_;

    void initWindow();

    void mainLoop();
};
