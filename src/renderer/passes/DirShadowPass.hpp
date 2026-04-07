//
// Created by johnny on 4/5/26.
//

#pragma once
#include "common/VulkanInclude.hpp"
#include "scene/MeshInstance.hpp"
#include <memory>
#include <vector>

class GraphicsPipeline;
class ShadowMap;
class Mesh;
struct DirLightView;

struct DirShadowPass {
    explicit DirShadowPass(ShadowMap &shadowMap);

    /// Renders all mesh instances into the shadow map depth image from the light's POV,
    /// then issues the depth-attachment → shader-read barrier for the lighting pass.
    void execute(vk::CommandBuffer cmd,
                 const GraphicsPipeline &pipeline,
                 const std::vector<MeshInstance> &meshInstances,
                 const DirLightView &dirLight) const;

private:
    ShadowMap &shadowMap_;
};
