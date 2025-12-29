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
        createSwapChain();
        createImageViews();
    }

    ~SwapChain();

    void recreate(); // call on resize

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    static bool isDeviceAdequate(VkPhysicalDevice device, VkSurfaceKHR surface);

    VkFormat getSwapChainImageFormat() const { return swapChainImageFormat_; }

    void createFramebuffers(VkRenderPass renderPass);

private:
    VulkanContext &context_;
    VkSwapchainKHR swapChain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages_;
    VkFormat swapChainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapChainExtent_ = {0, 0};
    GLFWwindow *window_;
    std::vector<VkImageView> swapChainImageViews_;
    std::vector<VkFramebuffer> swapChainFramebuffers_;


    void createImageViews();

    void createSwapChain();


    static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);
};
