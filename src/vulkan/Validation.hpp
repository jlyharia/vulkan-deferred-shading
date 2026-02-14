//
// Created by johnny on 12/22/25.
//
#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>

class Validation {
public:
    Validation(bool enableLayers = true);

    ~Validation() = default;

    bool isEnabled() const { return enableLayers_; }
    const std::vector<const char *> &getValidationLayers() const { return validationLayers_; }

    //Populate VkDebugUtilsMessengerCreateInfoEXT
    // void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
    void populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT &createInfo);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData
        );

    bool checkLayerSupport();

private:
    bool enableLayers_;
    const std::vector<const char *> validationLayers_ = {
        "VK_LAYER_KHRONOS_validation"
    };
};