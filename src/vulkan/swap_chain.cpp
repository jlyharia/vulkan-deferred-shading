//
// Created by johnny on 12/25/25.
//

#include "swap_chain.hpp"
#include "VulkanContext.hpp"
#include <algorithm>
#include <iostream>
#include <limits>

namespace {
// VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
//     for (const auto &availableFormat : availableFormats) {
//         if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace ==
//             VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
//             return availableFormat;
//         }
//     }
//
//     return availableFormats[0];
// }

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
    // Look for B8G8R8A8_SRGB + SRGB_NONLINEAR
    auto it = std::find_if(availableFormats.begin(), availableFormats.end(), [](const auto &f) {
        return f.format == vk::Format::eB8G8R8A8Srgb &&
               f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });

    // If found, return it; otherwise, return the first available format
    return (it != availableFormats.end()) ? *it : availableFormats[0];
}

// VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
//     // 1. Try Mailbox (Best for uncapped FPS without tearing),
//     // Many GPU doesn't support mailbox
//     for (const auto &mode : availablePresentModes) {
//         if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
//             std::cout << "-- Present mode found: VK_PRESENT_MODE_MAILBOX_KHR" << std::endl;
//             return mode;
//         }
//     }
//
//     // 2. Try Immediate (Absolute fastest, but causes screen tearing)
//     for (const auto &mode : availablePresentModes) {
//         if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
//             std::cout << "-- Present mode found: VK_PRESENT_MODE_IMMEDIATE_KHR" << std::endl;
//             return mode;
//         }
//     }
//
//     // 3. Fallback to FIFO (Always supported, locks to 60/144Hz)
//     std::cout << "-- Present mode default to: VK_PRESENT_MODE_FIFO_KHR" << std::endl;
//     return VK_PRESENT_MODE_FIFO_KHR;
// }

vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes) {
    // Triple Buffering (Mailbox) is preferred for performance without tearing
    auto it = std::find(availablePresentModes.begin(), availablePresentModes.end(), vk::PresentModeKHR::eMailbox);

    if (it != availablePresentModes.end()) {
        return *it;
    }

    // Standard V-Sync (FIFO) is guaranteed to be supported by the Vulkan spec
    return vk::PresentModeKHR::eFifo;
}
}

SwapChain::~SwapChain() {
    cleanup();
}

void SwapChain::cleanup() {
    auto device = context_.getDevice();

    // Using .destroy() instead of vkDestroy...
    if (depthImageView)
        device.destroyImageView(depthImageView);
    if (depthImage)
        device.destroyImage(depthImage);
    if (depthImageMemory)
        device.freeMemory(depthImageMemory);

    for (auto framebuffer : swapChainFramebuffers_) {
        device.destroyFramebuffer(framebuffer);
    }
    swapChainFramebuffers_.clear();

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
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(context_.getPhysicalDevice(),
                                                                     context_.getSurface());

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

        vk::Extent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                        capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void SwapChain::createImageViews() {
    swapChainImageViews_.resize(swapChainImages_.size());
    for (uint32_t i = 0; i < swapChainImages_.size(); i++) {
        swapChainImageViews_[i] = createImageView(swapChainImages_[i], swapChainImageFormat_,
                                                  vk::ImageAspectFlagBits::eColor);
    }
}

vk::ImageView SwapChain::createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags) const {
    auto viewInfo = vk::ImageViewCreateInfo()
                    .setImage(image)
                    .setViewType(vk::ImageViewType::e2D)
                    .setFormat(format)
                    .setSubresourceRange({aspectFlags, 0, 1, 0, 1});

    return context_.getDevice().createImageView(viewInfo);
}

void SwapChain::createFramebuffers(vk::RenderPass renderPass) {
    swapChainFramebuffers_.resize(swapChainImageViews_.size());

    for (size_t i = 0; i < swapChainImageViews_.size(); i++) {
        std::array<vk::ImageView, 2> attachments = {
            swapChainImageViews_[i],
            depthImageView
        };

        auto framebufferInfo = vk::FramebufferCreateInfo()
                               .setRenderPass(renderPass)
                               .setAttachments(attachments)
                               .setWidth(swapChainExtent_.width)
                               .setHeight(swapChainExtent_.height)
                               .setLayers(1);

        swapChainFramebuffers_[i] = context_.getDevice().createFramebuffer(framebufferInfo);
    }
}

void SwapChain::createDepthResources() {
    vk::Format depthFormat = findDepthFormat();

    createImage(swapChainExtent_.width, swapChainExtent_.height, depthFormat,
                vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment,
                vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage, depthImageMemory);

    depthImageView = createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
}

vk::Format SwapChain::findDepthFormat() {
    return swapChainDepthFormat_ = context_.findSupportedFormat(
               {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
               vk::ImageTiling::eOptimal,
               vk::FormatFeatureFlagBits::eDepthStencilAttachment
               );
}

void SwapChain::createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling,
                            vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Image &image,
                            vk::DeviceMemory &imageMemory) const {

    auto imageInfo = vk::ImageCreateInfo()
                     .setImageType(vk::ImageType::e2D)
                     .setExtent({width, height, 1})
                     .setMipLevels(1)
                     .setArrayLayers(1)
                     .setFormat(format)
                     .setTiling(tiling)
                     .setInitialLayout(vk::ImageLayout::eUndefined)
                     .setUsage(usage)
                     .setSamples(vk::SampleCountFlagBits::e1)
                     .setSharingMode(vk::SharingMode::eExclusive);

    image = context_.getDevice().createImage(imageInfo);

    vk::MemoryRequirements memRequirements = context_.getDevice().getImageMemoryRequirements(image);

    auto allocInfo = vk::MemoryAllocateInfo()
                     .setAllocationSize(memRequirements.size)
                     .setMemoryTypeIndex(context_.findMemoryType(memRequirements.memoryTypeBits, properties));

    imageMemory = context_.getDevice().allocateMemory(allocInfo);
    context_.getDevice().bindImageMemory(image, imageMemory, 0);
}