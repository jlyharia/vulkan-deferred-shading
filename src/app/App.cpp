//
// Created by johnny on 12/21/25.
//

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "App.hpp"

#include <stdexcept>

App::App(uint32_t width, uint32_t height, const char *title)
    : width_(width), height_(height), title_(title) {
}

App::~App() {
    vulkanContext_.reset(); // Vulkan RAII cleanup
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
    vulkanContext_ = std::make_unique<VulkanContext>(window_);
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
