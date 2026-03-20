//
// Created by johnny on 12/25/25.
//

#include "SwapChain.hpp"
#include "VulkanContext.hpp"
#include "VulkanUtils.hpp"

#include <algorithm>
#include <iostream>
#include <limits>

namespace {

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
    // Look for B8G8R8A8_SRGB + SRGB_NONLINEAR
    auto it = std::find_if(availableFormats.begin(), availableFormats.end(), [](const auto &f) {
        return f.format == vk::Format::eB8G8R8A8Srgb && f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });

    // If found, return it; otherwise, return the first available format
    return (it != availableFormats.end()) ? *it : availableFormats[0];
}


vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes) {
    // Triple Buffering (Mailbox) is preferred for performance without tearing
    auto it = std::find(availablePresentModes.begin(), availablePresentModes.end(), vk::PresentModeKHR::eMailbox);

    if (it != availablePresentModes.end()) {
        return *it;
    }

    // Standard V-Sync (FIFO) is guaranteed to be supported by the Vulkan spec
    return vk::PresentModeKHR::eFifo;
    // return vk::PresentModeKHR::eImmediate;
}
} // namespace

SwapChain::~SwapChain() { cleanup(); }

void SwapChain::cleanup() {
    auto device = context_.getDevice();
    auto allocator = context_.getVmaAllocator();
    // Using .destroy() instead of vkDestroy...
    if (depthImageView) {
        device.destroyImageView(depthImageView);
        depthImageView = nullptr;
    }

    if (depthImage_) {
        // This destroys BOTH the image and the memory allocation in one go
        vmaDestroyImage(allocator, depthImage_, depthImageAllocation);
        depthImage_ = nullptr;
    }

    for (auto imageView : swapChainImageViews_) {
        device.destroyImageView(imageView);
    }
    swapChainImageViews_.clear();

    if (swapChain_) {
        device.destroySwapchainKHR(swapChain_);
        swapChain_ = nullptr;
    }
}

bool SwapChain::isDeviceAdequate(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
    return !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
}

SwapChain::SwapChainSupportDetails SwapChain::querySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
    SwapChainSupportDetails details;
    // Single-call retrieval thanks to vulkan.hpp
    details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
    details.formats = device.getSurfaceFormatsKHR(surface);
    details.presentModes = device.getSurfacePresentModesKHR(surface);
    return details;
}

void SwapChain::createSwapChain() {
    SwapChainSupportDetails swapChainSupport =
        querySwapChainSupport(context_.getPhysicalDevice(), context_.getSurface());

    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    auto createInfo = vk::SwapchainCreateInfoKHR()
                      .setSurface(context_.getSurface())
                      .setMinImageCount(imageCount)
                      .setImageFormat(surfaceFormat.format)
                      .setImageColorSpace(surfaceFormat.colorSpace)
                      .setImageExtent(extent)
                      .setImageArrayLayers(1)
                      .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

    auto indices = context_.findQueueFamilies(context_.getPhysicalDevice());
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.setImageSharingMode(vk::SharingMode::eConcurrent)
                  .setQueueFamilyIndexCount(2)
                  .setPQueueFamilyIndices(queueFamilyIndices);
    } else {
        createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
    }

    createInfo.setPreTransform(swapChainSupport.capabilities.currentTransform)
              .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
              .setPresentMode(presentMode)
              .setClipped(true);

    swapChain_ = context_.getDevice().createSwapchainKHR(createInfo);

    // vulkan.hpp returns a vector directly
    swapChainImages_ = context_.getDevice().getSwapchainImagesKHR(swapChain_);
    swapChainImageFormat_ = surfaceFormat.format;
    swapChainExtent_ = extent;
}

vk::Extent2D SwapChain::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities) const {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window_, &width, &height);

        vk::Extent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

        actualExtent.width =
            std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height =
            std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void SwapChain::createImageViews() {
    swapChainImageViews_.resize(swapChainImages_.size());
    for (uint32_t i = 0; i < swapChainImages_.size(); i++) {
        swapChainImageViews_[i] =
            vk_util::createImageView(context_.getDevice(), swapChainImages_[i], swapChainImageFormat_,
                                     vk::ImageAspectFlagBits::eColor);
    }
}


void SwapChain::createDepthResources() {
    const vk::Format depthFormat = findDepthFormat();

    vk_util::createImage(context_.getVmaAllocator(),
                         swapChainExtent_.width,
                         swapChainExtent_.height,
                         depthFormat,
                         vk::ImageTiling::eOptimal,
                         vk::ImageUsageFlagBits::eDepthStencilAttachment,
                         VMA_MEMORY_USAGE_GPU_ONLY,
                         // vk::MemoryPropertyFlagBits::eDeviceLocal,
                         depthImage_,
                         depthImageAllocation);

    depthImageView = vk_util::createImageView(context_.getDevice(), depthImage_, depthFormat,
                                              vk::ImageAspectFlagBits::eDepth);
}

vk::Format SwapChain::findDepthFormat() {
    return swapChainDepthFormat_ = context_.findSupportedFormat(
               {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
               vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

