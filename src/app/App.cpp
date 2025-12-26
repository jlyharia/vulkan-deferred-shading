//
// Created by johnny on 12/21/25.
//

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "App.hpp"

#include <stdexcept>

#include "vulkan/Swapchain.hpp"

App::App(int width, int height, const char *title)
    : width_(width), height_(height), title_(title) {
}

App::~App() {
    swapchain_.reset();
    vulkanContext_.reset(); // Vulkan RAII cleanup, // calls ~VulkanContext()
    if (window_) glfwDestroyWindow(window_);
    glfwTerminate();
}

void App::initWindow() {
    if (!glfwInit()) throw std::runtime_error("Failed to init GLFW");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window_ = glfwCreateWindow(width_, height_, title_, nullptr, nullptr);
    if (!window_) throw std::runtime_error("Failed to create GLFW window");

    // Create Vulkan context after window is ready
    vulkanContext_ = std::make_unique<VulkanContext>(window_, true);
    swapchain_ = std::make_unique<Swapchain>(*vulkanContext_, window_);
}

void App::mainLoop() {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
    }
}

void App::run() {
    initWindow();
    mainLoop();
}
