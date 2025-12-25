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
 * VulkanContext = VkInstance + VkPhysicalDevice + VkDevice + VkQueues
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

        bool isComplete() {
            return graphicsFamily.has_value();
        }
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
};
