//
// Created by johnny on 12/21/25.
//

#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <memory>
#include <vector>
#include "Validation.hpp"

class VulkanContext {
public:
    VulkanContext(GLFWwindow *window, bool enableValidation = true);

    ~VulkanContext();

    VulkanContext(const VulkanContext &) = delete;

    VulkanContext &operator=(const VulkanContext &) = delete;

    VkInstance getInstance() const { return instance_; }

private:
    GLFWwindow *window_;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;

    std::unique_ptr<Validation> validation_;

    void createInstance();

    void setupDebugMessenger();

    std::vector<const char *> getRequiredExtensions();

    VkResult CreateDebugUtilsMessengerEXT(const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                          VkDebugUtilsMessengerEXT *pDebugMessenger);

    void DestroyDebugUtilsMessengerEXT(VkDebugUtilsMessengerEXT debugMessenger);
};
