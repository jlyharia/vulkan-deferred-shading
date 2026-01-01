//
// Created by johnny on 12/31/25.
//

#pragma once

namespace engine {
    // We use 'inline' so it can be included in multiple files without linker errors
    inline constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    // You can also put other engine-wide settings here later
    inline constexpr bool ENABLE_VALIDATION_LAYERS = true;
}