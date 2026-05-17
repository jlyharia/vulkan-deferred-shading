//
// Created by johnny on 4/5/26.
//

#pragma once
#include <array>
#include "common/Config.hpp"
#include "common/VulkanInclude.hpp"
#include "scene/DirLightView.hpp"
#include "scene/MeshInstance.hpp"
#include <vector>

class GraphicsPipeline;
class ShadowMap;

struct DirShadowPass {
    explicit DirShadowPass(ShadowMap &shadowMap);

    /// Renders all mesh instances into each cascade layer of the shadow map depth array image.
    void execute(vk::CommandBuffer cmd,
                 const GraphicsPipeline &pipeline,
                 const std::vector<MeshInstance> &meshInstances,
                 const std::array<CascadeData, engineConfig::NUM_CASCADES> &cascades) const;

private:
    ShadowMap &shadowMap_;
};
