//
// Created by johnny on 12/31/25.
//

#pragma once
#include <cstdint>
#include "VulkanInclude.hpp"

namespace engineConfig {
// We use 'inline' so it can be included in multiple files without linker errors
inline constexpr int MAX_FRAMES_IN_FLIGHT = 2;
inline constexpr int NUM_CASCADES = 4;
inline constexpr int SHADOW_MAP_SIZE = 2048;

// You can also put other engine-wide settings here later
inline constexpr bool ENABLE_VALIDATION_LAYERS = true;

inline constexpr int MAIN_WINDOW_HEIGHT = 2160;
inline constexpr int MAIN_WINDOW_WIDTH = 3840;

inline constexpr float DEFAULT_GUI_FONT = 3.0f;

inline constexpr uint32_t DEFAULT_VK_API_VERSION = VK_API_VERSION_1_3;

}


namespace DescriptorSets {
constexpr uint32_t GLOBAL_SET = 0;   // Set 0: Camera, Time, Lights, Instance SSBO
constexpr uint32_t MATERIAL_SET = 1; // Set 1: Albedo, Normal, MR
constexpr uint32_t LIGHTING_INPUTS_SET = 2;  // Set 2: G-buffer RTs + SSAO + shadow map (lighting pass inputs)
constexpr uint32_t SSAO_SET = 3;
}
