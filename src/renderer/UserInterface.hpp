//
// Created by johnny on 2/18/26.
//

#pragma once
#include "common/VulkanInclude.hpp"
#include "vulkan/VulkanContext.hpp"

class SwapChain;
class VulkanContext;

// https://youtu.be/drHKzbu6uC0?si=XDnBJw5lOuRjfd6a
class UserInterface {
public:
    UserInterface(VulkanContext &context, SwapChain &swapChain, GLFWwindow *window);

    ~UserInterface();

    void beginFrame();
    void endFrame();
    // This records the UI commands into your existing command buffer
    void recordCommands(vk::CommandBuffer cmd, uint32_t imageIndex) const;

private:
    VulkanContext &context_;
    SwapChain &swapChain_;
    vk::DescriptorPool imguiPool_;

    void createDescriptorPool();
};