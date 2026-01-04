//
// Created by johnny on 12/25/25.
//

#pragma once
#include <vector>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>


class VulkanContext;

class SwapChain {
public:
    SwapChain(VulkanContext &context, GLFWwindow *window)
        : context_(context), window_(window) {
        // createSwapChain();
        // createImageViews();
        init();
    }

    void recreate(VkRenderPass renderPass) {
        cleanup(); // Clean up old handles
        init(); // Create new handles with new window size
        createFramebuffers(renderPass); // Re-link to the renderpass
    }

    ~SwapChain();

    // Disable copying - Swapchains manage heavy GPU resources
    SwapChain(const SwapChain &) = delete;

    SwapChain &operator=(const SwapChain &) = delete;

    // The logic to "reset" the swapchain


    void cleanup(); // Logic moved out of destructor


    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    static bool isDeviceAdequate(VkPhysicalDevice device, VkSurfaceKHR surface);

    // Getters
    VkFormat getFormat() const { return swapChainImageFormat_; }
    VkExtent2D getExtent() const { return swapChainExtent_; }
    VkSwapchainKHR getHandle() const { return swapChain_; }
    const std::vector<VkImageView> &getImageViews() const { return swapChainImageViews_; }

    std::vector<VkFramebuffer> getFramebuffers() { return swapChainFramebuffers_; }

    void createFramebuffers(VkRenderPass renderPass);

    [[nodiscard]] const std::vector<VkImage> &getImages() const { return swapChainImages_; }

private:
    VulkanContext &context_;
    VkSwapchainKHR swapChain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages_;
    VkFormat swapChainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapChainExtent_ = {0, 0};
    GLFWwindow *window_;
    std::vector<VkImageView> swapChainImageViews_;
    std::vector<VkFramebuffer> swapChainFramebuffers_;


    void init() {
        createSwapChain();
        createImageViews();
    }

    void createImageViews();

    void createSwapChain();


    static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);
};
