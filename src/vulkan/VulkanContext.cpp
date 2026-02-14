#include "VulkanContext.hpp"
#include "Validation.hpp"
#include "swap_chain.hpp"
#include <iostream>
#include <map>
#include <set>

// This macro instantiates the storage for the global dispatcher in this translation unit.
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

const std::vector<const char *> VulkanContext::deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

VulkanContext::VulkanContext(GLFWwindow *window, bool enableValidation)
    : window_(window), validation_(std::make_unique<Validation>(enableValidation)) {

    // 1. Initialize global dispatcher with the base loader
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    if (validation_->isEnabled() && !validation_->checkLayerSupport()) {
        throw std::runtime_error("Validation layers requested, but not available!");
    }

    createInstance();

    // 2. Patch global dispatcher with Instance pointers
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance_);

    if (validation_->isEnabled()) {
        setupDebugMessenger();
    }

    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
}

VulkanContext::~VulkanContext() {
    // Note: No 'dldy' passed anymore; functions use the global dispatcher automatically.
    // if (vkDevice_) {
    //     vkDevice_.waitIdle();
    // }
    //
    // vkDestroySurfaceKHR(instance_, surface_, nullptr);
    //
    // if (instance_ && debugMessenger_) {
    //     instance_.destroyDebugUtilsMessengerEXT(debugMessenger_);
    // }
    //
    // if (vkDevice_) {
    //     vkDevice_.destroy();
    // }
    //
    // if (instance_ && surface_) {
    //     instance_.destroySurfaceKHR(surface_);
    // }
    //
    // if (instance_) {
    //     instance_.destroy();
    // }
    std::cerr << "[Destructor] VulkanContext starting..." << std::endl;
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    std::cerr << "[Destructor] VulkanContext-vkDestroySurfaceKHR..." << std::endl;
    if (this->vkDevice_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(vkDevice_);
        vkDestroyDevice(vkDevice_, nullptr);
        std::cerr << "[Destructor] VulkanContext-vkDevice_..." << std::endl;
    }
    if (validation_->isEnabled()) {
        DestroyDebugUtilsMessengerEXT(debugMessenger_);
    }

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
    }


}

void VulkanContext::createInstance() {
    vk::ApplicationInfo appInfo{
        "Hello Triangle",
        VK_MAKE_API_VERSION(0, 1, 0, 0),
        "No Engine",
        VK_MAKE_API_VERSION(0, 1, 0, 0),
        vk::ApiVersion13
    };

    auto extensions = getRequiredExtensions();

    // 1. Create a persistent local copy of the layer names
    std::vector<const char *> layers;
    if (validation_->isEnabled()) {
        if (validation_->checkLayerSupport()) {
            layers = validation_->getValidationLayers();
        } else {
            throw std::runtime_error("Validation layers requested, but not available!");
        }
    }

    vk::InstanceCreateInfo createInfo;
    createInfo.setPApplicationInfo(&appInfo)
              .setPEnabledExtensionNames(extensions);

    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (validation_->isEnabled()) {
        // 2. Pass the local 'layers' vector
        createInfo.setPEnabledLayerNames(layers);

        validation_->populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.setPNext(&debugCreateInfo);
    }

    // 3. Create the instance
    instance_ = vk::createInstance(createInfo);
}

void VulkanContext::pickPhysicalDevice() {
    auto devices = instance_.enumeratePhysicalDevices();
    if (devices.empty()) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::multimap<int, vk::PhysicalDevice> candidates;

    for (const auto &device : devices) {
        int score = rateDeviceSuitability(device);
        candidates.insert(std::make_pair(score, device));
    }

    if (candidates.rbegin()->first > 0) {
        physicalDevice_ = candidates.rbegin()->second;

        vk::PhysicalDeviceProperties props = physicalDevice_.getProperties();
        std::cout << "-- Selected GPU: " << props.deviceName.data()
            << " (Score: " << candidates.rbegin()->first << ")" << std::endl;
    } else {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
}

int VulkanContext::rateDeviceSuitability(vk::PhysicalDevice device) {
    if (!isDeviceSuitable(device))
        return 0;

    int score = 0;
    vk::PhysicalDeviceProperties props = device.getProperties();
    vk::PhysicalDeviceFeatures features = device.getFeatures();

    if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
        score += 1000;
    }

    score += props.limits.maxImageDimension2D;

    if (features.geometryShader)
        score += 50;
    if (features.samplerAnisotropy)
        score += 100;

    return score;
}

void VulkanContext::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        queueCreateInfos.push_back({{}, queueFamily, 1, &queuePriority});
    }

    vk::PhysicalDeviceVulkan13Features features13;
    features13.setDynamicRendering(true).setSynchronization2(true);

    vk::PhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.setSamplerAnisotropy(true);

    vk::DeviceCreateInfo createInfo;
    createInfo.setQueueCreateInfos(queueCreateInfos)
              .setPEnabledFeatures(&deviceFeatures)
              .setPEnabledExtensionNames(deviceExtensions)
              .setPNext(&features13);

    if (validation_->isEnabled()) {
        createInfo.setPEnabledLayerNames(validation_->getValidationLayers());
    }

    vkDevice_ = physicalDevice_.createDevice(createInfo);

    // 3. Final Patch: Initialize global dispatcher with Device pointers for speed
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkDevice_);

    graphicsQueue_ = vkDevice_.getQueue(indices.graphicsFamily.value(), 0);
    presentQueue_ = vkDevice_.getQueue(indices.presentFamily.value(), 0);
}

bool VulkanContext::isDeviceSuitable(vk::PhysicalDevice device) const {
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        // Note: Check if your SwapChain class also uses the global dispatcher!
        swapChainAdequate = SwapChain::isDeviceAdequate(static_cast<VkPhysicalDevice>(device),
                                                        static_cast<VkSurfaceKHR>(surface_));
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

QueueFamilyIndices VulkanContext::findQueueFamilies(vk::PhysicalDevice device) const {
    QueueFamilyIndices indices;
    auto queueFamilies = device.getQueueFamilyProperties();

    int i = 0;
    for (const auto &queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsFamily = i;
        }

        if (device.getSurfaceSupportKHR(i, surface_)) {
            indices.presentFamily = i;
        }

        if (indices.isComplete())
            break;
        i++;
    }
    return indices;
}

void VulkanContext::createSurface() {
    VkSurfaceKHR rawSurface;
    if (glfwCreateWindowSurface(instance_, window_, nullptr, &rawSurface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
    surface_ = vk::SurfaceKHR(rawSurface);
}

void VulkanContext::setupDebugMessenger() {
    vk::DebugUtilsMessengerCreateInfoEXT createInfo;
    validation_->populateDebugMessengerCreateInfo(createInfo);
    debugMessenger_ = instance_.createDebugUtilsMessengerEXT(createInfo);
}

bool VulkanContext::checkDeviceExtensionSupport(vk::PhysicalDevice device) {
    auto availableExtensions = device.enumerateDeviceExtensionProperties();
    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto &extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }
    return requiredExtensions.empty();
}

std::vector<const char *> VulkanContext::getRequiredExtensions() const {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (validation_->isEnabled())
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    return extensions;
}

uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const {
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice_.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypes.size(); i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

vk::Format VulkanContext::findSupportedFormat(
    const std::vector<vk::Format> &candidates,
    vk::ImageTiling tiling,
    vk::FormatFeatureFlags features) const {
    for (vk::Format format : candidates) {
        vk::FormatProperties props = physicalDevice_.getFormatProperties(format);

        if (tiling == vk::ImageTiling::eLinear) {
            if ((props.linearTilingFeatures & features) == features)
                return format;
        } else if (tiling == vk::ImageTiling::eOptimal) {
            if ((props.optimalTilingFeatures & features) == features)
                return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}

void VulkanContext::DestroyDebugUtilsMessengerEXT(VkDebugUtilsMessengerEXT debugMessenger) const {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance_, "vkDestroyDebugUtilsMessengerEXT");
    if (func) {
        func(instance_, debugMessenger, nullptr);
    }
}