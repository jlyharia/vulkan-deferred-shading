#pragma once
#include "CompiledPass.hpp"
#include "RGTextureAccess.hpp"
#include "RGTexture.hpp"
#include "TextureState.hpp"
#include "common/VulkanInclude.hpp"

#include <optional>
#include <unordered_map>
#include <vector>



/**
 * owns the registry and passes, runs compile and execute.
 */
class RenderGraph {
public:
    void addPass(RGPass pass);
    void registerTexture(RGTexture tex);

    void compile(); // topo sort + derive barrier list per pass
    void execute(vk::CommandBuffer cmd);
    void reset();  // clear passes and registry before rebuilding each frame

private:
    std::unordered_map<std::string, RGTexture> textureRegistry;
    std::vector<RGPass> passes; // unsorted
    std::vector<CompiledPass> compiledPass; // sorted + barriers baked in

    // Returns a barrier for tAccess, or nullopt if no transition is needed.
    // textureStateMap is updated in place to track current layout/stage/access per texture.
    std::optional<vk::ImageMemoryBarrier2> makeBarrier(const RGTextureAccess &tAccess,
                                                       std::unordered_map<std::string, TextureState> &textureStateMap) const;
};
