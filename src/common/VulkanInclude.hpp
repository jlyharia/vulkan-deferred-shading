//
// Created by johnny on 2/23/26.
//

#pragma once

// 1. Setup the Dispatcher Macro
#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#endif

// 2. Include Volk (The dynamic loader)
#include <volk.h>

// 3. Include Vulkan-Hpp (The C++ wrapper)
#include <vulkan/vulkan.hpp>

// 4. Include VMA (But NOT the implementation)
#include <vk_mem_alloc.h>

// 5. Include GLFW (Tells GLFW to use Vulkan)
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>