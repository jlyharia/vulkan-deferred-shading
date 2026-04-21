#pragma once

#include "common/VulkanInclude.hpp"

struct TextureState {
    vk::ImageLayout layout;
    vk::PipelineStageFlags2 stage;
    vk::AccessFlags2 access;
};
