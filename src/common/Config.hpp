//
// Created by johnny on 12/31/25.
//

#pragma once

namespace engineConfig {
// We use 'inline' so it can be included in multiple files without linker errors
inline constexpr int MAX_FRAMES_IN_FLIGHT = 2;

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
constexpr uint32_t GBUFFER_SET = 2;  // Set 2: G-buffer textures (deferred lighting pass)
constexpr uint32_t SSAO_SET = 3;
}
