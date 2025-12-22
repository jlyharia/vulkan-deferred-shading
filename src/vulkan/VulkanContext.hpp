//
// Created by johnny on 12/21/25.
//

#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

class VulkanContext {
public:
    VulkanContext(GLFWwindow *window);

    ~VulkanContext();

    VkInstance getInstance() const { return instance_; }

private:
    void createInstance();

    void destroyInstance();

    GLFWwindow *window_;
    VkInstance instance_ = VK_NULL_HANDLE;
};
