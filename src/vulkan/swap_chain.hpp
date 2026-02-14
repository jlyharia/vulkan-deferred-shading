//
// Created by johnny on 12/25/25.
//
#pragma once
#include <vector>
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

class VulkanContext;

class SwapChain {
public:
    SwapChain(VulkanContext &context, GLFWwindow *window)
        : context_(context), window_(window) {
        init();
    }

    ~SwapChain();

    // Recreate now uses vk::RenderPass
    void recreate(vk::RenderPass renderPass) {
        cleanup();
        init();
        createFramebuffers(renderPass);
    }

    void cleanup();

    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    // Use vk:: types and global dispatcher
    static bool isDeviceAdequate(vk::PhysicalDevice device, vk::SurfaceKHR surface);
    static SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface);

    // Getters
    vk::Format getColorFormat() const { return swapChainImageFormat_; }
    vk::Extent2D getExtent() const { return swapChainExtent_; }
    vk::SwapchainKHR getHandle() const { return swapChain_; }
    const std::vector<vk::ImageView> &getImageViews() const { return swapChainImageViews_; }
    [[nodiscard]] const std::vector<vk::Framebuffer> &getFramebuffers() const { return swapChainFramebuffers_; }
    [[nodiscard]] vk::Format getDepthFormat() const { return swapChainDepthFormat_; }

    void createFramebuffers(vk::RenderPass renderPass);

private:
    VulkanContext &context_;
    GLFWwindow *window_;

    vk::SwapchainKHR swapChain_;
    std::vector<vk::Image> swapChainImages_; // Changed to vk::Image
    vk::Format swapChainImageFormat_;
    vk::Extent2D swapChainExtent_;

    std::vector<vk::ImageView> swapChainImageViews_;
    std::vector<vk::Framebuffer> swapChainFramebuffers_;

    // Depth Resources
    vk::Image depthImage;
    vk::DeviceMemory depthImageMemory;
    vk::ImageView depthImageView;
    vk::Format swapChainDepthFormat_;

    void init() {
        createSwapChain();
        createImageViews();
        createDepthResources();
    }

    void createImageViews();
    void createSwapChain();
    void createDepthResources();

    vk::ImageView createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags) const;

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities) const;

    // Helper for depth image creation
    void createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling,
                     vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties,
                     vk::Image &image, vk::DeviceMemory &imageMemory) const;

    vk::Format findDepthFormat();
};