#pragma once
#include "common/VulkanInclude.hpp"

struct RGTextureAccess {
    std::string name;
    vk::ImageLayout layout;          // layout the pass expects the texture to be in
    vk::PipelineStageFlags2 stage;   // which stage touches it
    vk::AccessFlags2 access;         // eShaderSampledRead, eColorAttachmentWrite, etc.
};
