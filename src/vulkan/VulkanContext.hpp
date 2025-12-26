//
// Created by johnny on 12/21/25.
//

#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <memory>
#include <optional>
#include <vector>
#include "Validation.hpp"

/**
 * VulkanContext
 *
 * Owns the foundational, long-lived Vulkan objects that typically live
 * for the entire lifetime of the application.
 *
 * Responsibilities:
 *  - VkInstance
 *  - VkDebugUtilsMessengerEXT (validation / debug callbacks)
 *  - VkSurfaceKHR (window-system integration; required for device selection)
 *  - VkPhysicalDevice (GPU selection)
 *  - VkDevice (logical device)
 *  - VkQueue(s) (graphics / present)
 *  - Validation layer setup and lifetime management
 *
 * This class intentionally does NOT own short-lived or resize-dependent
 * resources such as swapchains, framebuffers, or render targets.
 */

class VulkanContext {
public:
    VulkanContext(GLFWwindow *window, bool enableValidation = true);

    ~VulkanContext();

    VulkanContext(const VulkanContext &) = delete;

    VulkanContext &operator=(const VulkanContext &) = delete;

    [[nodiscard]] VkInstance getInstance() const { return instance_; }

private:
    GLFWwindow *window_;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    std::unique_ptr<Validation> validation_;
    VkDevice vkDevice_;
    VkSurfaceKHR surface_;
    VkQueue graphicsQueue_;
    VkQueue presentQueue_;

    void createInstance();

    void setupDebugMessenger();

    std::vector<const char *> getRequiredExtensions();

    VkResult CreateDebugUtilsMessengerEXT(const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                          VkDebugUtilsMessengerEXT *pDebugMessenger);

    void DestroyDebugUtilsMessengerEXT(VkDebugUtilsMessengerEXT debugMessenger);

    void pickPhysicalDevice();

    void createLogicalDevice();

    bool isDeviceSuitable(VkPhysicalDevice device);

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    void createSurface();
};
