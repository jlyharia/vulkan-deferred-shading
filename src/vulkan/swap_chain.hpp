//
// Created by johnny on 12/25/25.
//
#pragma once

#include "common/VulkanInclude.hpp"
#include <vector>

class VulkanContext;

class SwapChain {
public:
    // Support details structure
    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    // Constructor & Destructor
    SwapChain(VulkanContext &context, GLFWwindow *window) : context_(context), window_(window) { init(); }
    ~SwapChain();

    // Lifecycle methods
    void recreate() {
        cleanup();
        init();
        // createFramebuffers(renderPass);
    }

    void cleanup();

    // Static utility methods
    static bool isDeviceAdequate(vk::PhysicalDevice device, vk::SurfaceKHR surface);
    static SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface);

    // Getters - Swap chain properties
    [[nodiscard]] vk::SwapchainKHR getHandle() const { return swapChain_; }
    [[nodiscard]] vk::Format getColorFormat() const { return swapChainImageFormat_; }
    [[nodiscard]] vk::Format getDepthFormat() const { return swapChainDepthFormat_; }
    [[nodiscard]] vk::Extent2D getExtent() const { return swapChainExtent_; }

    // Getters - Resources
    [[nodiscard]] std::vector<vk::Image> getImages() const { return swapChainImages_; }
    [[nodiscard]] const std::vector<vk::ImageView> &getImageViews() const { return swapChainImageViews_; }
    [[nodiscard]] vk::ImageView getDepthImageView() const { return depthImageView; }
    [[nodiscard]] vk::Image getDepthImage() const { return depthImage_; }

private:
    // Context references
    VulkanContext &context_;
    GLFWwindow *window_;

    // Swap chain resources
    vk::SwapchainKHR swapChain_;
    std::vector<vk::Image> swapChainImages_;
    vk::Format swapChainImageFormat_;
    vk::Extent2D swapChainExtent_;
    std::vector<vk::ImageView> swapChainImageViews_;

    // Depth resources
    vk::Image depthImage_;
    VmaAllocation depthImageAllocation;
    vk::ImageView depthImageView;
    vk::Format swapChainDepthFormat_;

    // Initialization
    void init() {
        createSwapChain();
        createImageViews();
        createDepthResources();
    }

    // Creation methods
    void createSwapChain();
    void createImageViews();
    void createDepthResources();

    // Helper methods
    // [[nodiscard]] vk::ImageView createImageView(vk::Image image, vk::Format format,
    //                                             vk::ImageAspectFlags aspectFlags) const;
    [[nodiscard]] vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities) const;
    vk::Format findDepthFormat();
    // void createImage(uint32_t width,
    //                  uint32_t height,
    //                  vk::Format format,
    //                  vk::ImageTiling tiling,
    //                  vk::ImageUsageFlags usage,
    //                  VmaMemoryUsage vmaUsage,
    //                  vk::Image &image,
    //                  VmaAllocation &allocation) const;
};