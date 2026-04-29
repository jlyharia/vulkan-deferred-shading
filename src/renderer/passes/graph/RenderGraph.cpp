#include "RenderGraph.hpp"

#include "CompiledPass.hpp"
#include "RGPass.hpp"
#include "RGTexture.hpp"
#include "TextureState.hpp"
#include "vulkan/VulkanUtils.hpp"
#include <string_view>

#include <cassert>
#include <queue>
#include <ranges>

void RenderGraph::addPass(RGPass pass) {
    passes.push_back(std::move(pass));
}

void RenderGraph::registerTexture(RGTexture tex) {
    textureRegistry[tex.name] = std::move(tex);
}

void RenderGraph::compile() {
    // texture name → pass name that writes it
    std::unordered_map<std::string, std::string> writerMap;
    for (auto &pass : passes)
        for (auto &w : pass.writeTextures)
            writerMap[w.name] = pass.name;

    // texture name → passes that read it
    std::unordered_map<std::string, std::vector<std::string>> textureToPassMap;
    for (auto &pass : passes)
        for (auto &r : pass.readTextures)
            textureToPassMap[r.name].push_back(pass.name);

    // in-degree = reads that have a producer in this graph
    std::unordered_map<std::string, size_t> inDegree;
    for (auto &pass : passes) {
        size_t count = 0;
        for (auto &r : pass.readTextures)
            if (writerMap.contains(r.name)) // "swapchain" → count = 0, skipped   
                count++;
        inDegree[pass.name] = count;
    }

    // name → index for O(1) lookup during sort and barrier derivation
    std::unordered_map<std::string, size_t> passIndex;
    for (size_t i = 0; i < passes.size(); ++i)
        passIndex[passes[i].name] = i;

    std::queue<std::string> sortQueue;
    for (auto &[name, deg] : inDegree)
        if (deg == 0)
            sortQueue.push(name);

    std::vector<std::string> sortedPassNames;
    while (!sortQueue.empty()) {
        auto top = sortQueue.front();
        sortQueue.pop();
        sortedPassNames.push_back(top);

        for (auto &w : passes[passIndex.at(top)].writeTextures) {
            for (auto &reader : textureToPassMap[w.name]) {
                if (--inDegree.at(reader) == 0)
                    sortQueue.push(reader);
            }
        }
    }
    assert(sortedPassNames.size() == passes.size() && "Render graph has a cycle");

    // initialize texture state from registry
    std::unordered_map<std::string, TextureState> textureStateMap;
    for (auto &[name, tex] : textureRegistry)
        textureStateMap[name] = {tex.initialLayout, vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone};

    // populate compiled in sorted order, deriving barriers per pass
    compiledPass.clear();
    for (auto &passName : sortedPassNames) {
        auto &pass = passes[passIndex.at(passName)];

        std::vector<vk::ImageMemoryBarrier2> barriers;

        for (auto &r : pass.readTextures)
            if (auto b = makeBarrier(r, textureStateMap))
                barriers.push_back(*b);
        for (auto &w : pass.writeTextures)
            if (auto b = makeBarrier(w, textureStateMap))
                barriers.push_back(*b);

        compiledPass.push_back({pass, std::move(barriers)});
    }
}


// Returns a barrier transitioning the texture to the layout the pass requires, or nullopt if none needed.
// textureStateMap holds each texture's current layout/stage/access and is updated in place as we walk
// sorted passes — so each call sees the state left by the previous pass that touched this texture.
std::optional<vk::ImageMemoryBarrier2> RenderGraph::makeBarrier(const RGTextureAccess &tAccess,
                                                                std::unordered_map<std::string, TextureState> &
                                                                textureStateMap)
const {
    auto stateIt = textureStateMap.find(tAccess.name);
    if (stateIt == textureStateMap.end())
        return std::nullopt; // imported resource not in registry — caller owns its transitions

    auto &cur = stateIt->second; // reference into map — assignment below updates it in place
    if (cur.layout == tAccess.layout && cur.stage == tAccess.stage && cur.access == tAccess.access)
        return std::nullopt; // already in the required layout/stage/access

    auto &tex = textureRegistry.at(tAccess.name);
    vk::ImageAspectFlags aspect = vk_util::imageAspect(tex.format);

    auto barrier = vk::ImageMemoryBarrier2{}
                   .setSrcStageMask(cur.stage) // what the GPU was last doing with this texture
                   .setSrcAccessMask(cur.access)
                   .setDstStageMask(tAccess.stage) // what this pass needs
                   .setDstAccessMask(tAccess.access)
                   .setOldLayout(cur.layout)
                   .setNewLayout(tAccess.layout)
                   .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                   .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                   .setImage(tex.image)
                   .setSubresourceRange({aspect, 0, 1, 0, 1});

    cur = {tAccess.layout, tAccess.stage, tAccess.access};
    // update reference in textureStateMap so next pass will see updated state
    return barrier;
}

void RenderGraph::reset() {
    passes.clear();
    textureRegistry.clear();
    compiledPass.clear();
}

void RenderGraph::execute(const vk::CommandBuffer cmd) {
    for (auto &[pass, barriers] : compiledPass) {
        if (!barriers.empty()) {
            cmd.pipelineBarrier2(vk::DependencyInfo{}.setImageMemoryBarriers(barriers));
        }
        assert(pass.execute && "RGPass registered without execute lambda");
        vk_util::cmdBeginLabel(cmd, pass.name);
        pass.execute(cmd);
        vk_util::cmdEndLabel(cmd);
    }
}