#pragma once
#include "common/VulkanInclude.hpp"

#include "RGTextureAccess.hpp"
#include <functional>

// A single node in the render graph. Declares which textures it reads and writes
// so compile() can derive execution order and barriers. execute is the actual
// GPU work recorded into the command buffer at frame time.
struct RGPass {
    std::string name;
    std::vector<RGTextureAccess> readTextures;  // textures consumed as shader inputs
    std::vector<RGTextureAccess> writeTextures; // textures written as attachments or storage
    std::function<void(vk::CommandBuffer)> execute;
};
