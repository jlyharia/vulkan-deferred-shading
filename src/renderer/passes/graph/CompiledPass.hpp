#pragma once
#include "RGPass.hpp"
#include "common/VulkanInclude.hpp"

// Sorted result of compile() — a pass plus the barriers that fire before it runs.
struct CompiledPass {
    RGPass pass;
    std::vector<vk::ImageMemoryBarrier2> barriers; // emitted before execute
};
