//
// Created by johnny on 12/22/25.
//

#include "Validation.hpp"
#include <iostream>
#include <cstring>
#include <vector>
#include <vulkan/vulkan.hpp>

Validation::Validation(bool enableLayers) : enableLayers_(enableLayers) {
}

void Validation::populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT &createInfo) {
    // No need to manually set sType or zero out memory;
    // the constructor and reset take care of that.

    createInfo.setMessageSeverity(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
    );

    createInfo.setMessageType(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral    |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
    );

    // Casting the callback to the required PFN type
    // Use the vk namespace function pointer type
    createInfo.setPfnUserCallback(reinterpret_cast<vk::PFN_DebugUtilsMessengerCallbackEXT>(debugCallback));
    createInfo.setPUserData(nullptr);
}

VKAPI_ATTR VkBool32 VKAPI_CALL Validation::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData
) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

bool Validation::checkLayerSupport() {
    std::cout << "VK_LAYER_PATH = " << std::getenv("VK_LAYER_PATH") << std::endl;
    std::cout << "LD_LIBRARY_PATH = " << std::getenv("LD_LIBRARY_PATH") << std::endl;
    auto availableLayers = vk::enumerateInstanceLayerProperties();

    for (const char* layerName : validationLayers_) {
        bool found = false;
        for (const auto& layerProperties : availableLayers) {
            // Compare the string inside the layerName field
            std::cout << layerProperties.layerName << std::endl;
            if (std::strcmp(layerName, layerProperties.layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "Requested validation layer not found: " << layerName << std::endl;
            return false;
        }
    }
    return true;
}
