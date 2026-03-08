//
// Created by johnny on 12/21/25.
//

#pragma once

#include "common/VulkanInclude.hpp"

#include <memory>
#include <optional>
#include <vector>
class Validation;

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

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class VulkanContext {
public:
    VulkanContext(GLFWwindow *window, bool enableValidation = true);

    ~VulkanContext();

    VulkanContext(const VulkanContext &) = delete;

    VulkanContext &operator=(const VulkanContext &) = delete;

    [[nodiscard]] VkInstance getInstance() const { return instance_; }

    static const std::vector<const char *> deviceExtensions;

    /**
     *
     * @return Logical Device
     */
    [[nodiscard]] vk::Device getDevice() const { return vkDevice_; }
    /**
     *
     * @return Physical Device
     */
    [[nodiscard]] vk::PhysicalDevice getPhysicalDevice() const { return physicalDevice_; }
    [[nodiscard]] vk::SurfaceKHR getSurface() const { return surface_; }

    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device) const;

    [[nodiscard]] vk::Queue getGraphicsQueue() const { return graphicsQueue_; }
    [[nodiscard]] vk::Queue getPresentQueue() const { return presentQueue_; }
    [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

    [[nodiscard]] vk::Format findSupportedFormat(const std::vector<vk::Format> &candidates,
                                                 vk::ImageTiling tiling,
                                                 vk::FormatFeatureFlags features) const;
    [[nodiscard]] VmaAllocator getVmaAllocator() const { return vmaAllocator_; }

    // Pool for main frame rendering (Commands that are reset every frame)
    [[nodiscard]] vk::CommandPool getMainCommandPool() const { return mainCommandPool_; }

    // Pool for loading/transfer (One-time submit commands)
    [[nodiscard]] vk::CommandPool getTransferCommandPool() const { return transferCommandPool_; }

    [[nodiscard]] QueueFamilyIndices getQueueIndices() const { return queueIndices_; };

private:
    GLFWwindow *window_;

    // Core Handles
    vk::Instance instance_;
    vk::PhysicalDevice physicalDevice_;
    vk::Device vkDevice_;

    // This replaces the need for the global storage macro
    // vk::DispatchLoaderDynamic dldy_;
    // Surface & Queues
    vk::SurfaceKHR surface_; // vulkan.hpp wrapper for VkSurfaceKHR
    vk::Queue graphicsQueue_; // Returned by vkDevice_.getQueue()
    vk::Queue presentQueue_;

    // Debugging
    vk::DebugUtilsMessengerEXT debugMessenger_;
    std::unique_ptr<Validation> validation_;
    // Memory Resources (VMA + vk::Buffer)
    VmaAllocator vmaAllocator_ = nullptr;
    vk::CommandPool mainCommandPool_;
    vk::CommandPool transferCommandPool_;
    // Store the indices as a member
    QueueFamilyIndices queueIndices_;
    void createInstance();

    void setupDebugMessenger();

    std::vector<const char *> getRequiredExtensions() const;

    VkResult CreateDebugUtilsMessengerEXT(const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                          VkDebugUtilsMessengerEXT *pDebugMessenger);

    void DestroyDebugUtilsMessengerEXT(VkDebugUtilsMessengerEXT debugMessenger) const;

    void pickPhysicalDevice();

    void createLogicalDevice();

    bool isDeviceSuitable(vk::PhysicalDevice device) const;

    void createSurface();

    static bool checkDeviceExtensionSupport(vk::PhysicalDevice device);
    int rateDeviceSuitability(vk::PhysicalDevice device);
    void initVmaAllocator();
    void createCommandPools();
};