#include "GraphicsPipeline.hpp"
#include "VulkanContext.hpp"
#include "../common/Vertex.hpp"
#include "SwapChain.hpp"
#include "GBuffer.hpp"
#include "common/PushConstantConstant.hpp"

#include <fstream>
#include <iostream>

namespace {
std::vector<char> readFile(const std::string &filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("failed to open file: " + filename);

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}
} // namespace

GraphicsPipeline::GraphicsPipeline(VulkanContext &context, SwapChain &swapChain,
                                   const std::vector<vk::DescriptorSetLayout> &descriptorSetLayouts,
                                   vk::DescriptorSetLayout ssaoBlurLayout)
    : context_(context), swapChain_(swapChain) {

    createPipelineLayout(descriptorSetLayouts);
    createSsaoBlurPipelineLayout(ssaoBlurLayout);
    createDirShadowPipelineLayout();

    vk::Format swapFormat = swapChain_.getColorFormat();

    unlitPipeline_ = buildPipeline({
        .vertSpv = "shaders/unlit/unlit.vert.spv",
        .fragSpv = "shaders/unlit/unlit.frag.spv",
        .colorFormats = {swapFormat},
    }, pipelineLayout_);

    // --- Deferred: G-buffer geometry pass (2 MRT color attachments + depth write) ---
    gbufferPipeline_ = buildPipeline({
        .vertSpv = "shaders/gbuffer/gbuffer.vert.spv",
        .fragSpv = "shaders/gbuffer/gbuffer.frag.spv",
        .colorFormats = {GBuffer::ALBEDO_METALLIC_FORMAT, GBuffer::NORMAL_ROUGHNESS_FORMAT},
    }, pipelineLayout_);

    // --- Deferred: lighting pass (fullscreen triangle, no vertex input, no depth) ---
    lightingPipeline_ = buildPipeline({
        .vertSpv = "shaders/lighting/lighting.vert.spv",
        .fragSpv = "shaders/lighting/lighting.frag.spv",
        .colorFormats = {swapFormat},
        .hasVertexInput = false,
        .depthTestEnable = false,
        .depthWriteEnable = false,
        .cullMode = vk::CullModeFlagBits::eNone,
    }, pipelineLayout_);

    // --- Deferred: forward overlay (light spheres — depth test ON, depth write OFF) ---
    overlayUnlitPipeline_ = buildPipeline({
        .vertSpv = "shaders/unlit/unlit.vert.spv",
        .fragSpv = "shaders/unlit/unlit.frag.spv",
        .colorFormats = {swapFormat},
        .depthTestEnable = true,
        .depthWriteEnable = false,
    }, pipelineLayout_);

    ssaoPipeline_ = buildPipeline({
        .vertSpv = "shaders/ssao/ssao.vert.spv",
        .fragSpv = "shaders/ssao/ssao.frag.spv",
        .colorFormats = {vk::Format::eR8Unorm},
        .hasVertexInput = false,
        .depthTestEnable = false,
        .depthWriteEnable = false,
        .cullMode = vk::CullModeFlagBits::eNone,
    }, pipelineLayout_);

    ssaoBlurPipeline_ = buildPipeline({
        .vertSpv = "shaders/ssaoblur/ssaoblur.vert.spv",
        .fragSpv = "shaders/ssaoblur/ssaoblur.frag.spv",
        .colorFormats = {vk::Format::eR8Unorm},
        .hasVertexInput = false,
        .depthTestEnable = false,
        .depthWriteEnable = false,
        .cullMode = vk::CullModeFlagBits::eNone,
    }, ssaoBlurPipelineLayout_);

    // --- Shadow: depth-only from light's perspective (no color attachment, front-face culling) ---
    dirShadowPipeline_ = buildPipeline({
        .vertSpv = "shaders/shadow/dirShadow.vert.spv",
        .colorFormats = {},
        .depthTestEnable = true,
        .depthWriteEnable = true,
        .depthBiasEnable = true,  // dynamic — set via cmd.setDepthBias() in DirShadowPass
        .cullMode = vk::CullModeFlagBits::eFront,
    }, dirShadowPipelineLayout_);
}

GraphicsPipeline::~GraphicsPipeline() {
    auto device = context_.getDevice();
    device.destroyPipeline(unlitPipeline_);
    device.destroyPipeline(gbufferPipeline_);
    device.destroyPipeline(lightingPipeline_);
    device.destroyPipeline(overlayUnlitPipeline_);
    device.destroyPipeline(ssaoPipeline_);
    device.destroyPipeline(ssaoBlurPipeline_);
    device.destroyPipeline(dirShadowPipeline_);
    device.destroyPipelineLayout(dirShadowPipelineLayout_);
    device.destroyPipelineLayout(ssaoBlurPipelineLayout_);
    device.destroyPipelineLayout(pipelineLayout_);
}

vk::Pipeline GraphicsPipeline::buildPipeline(const PipelineConfig &config,
                                             vk::PipelineLayout layout) const {
    vk::ShaderModule vertShaderModule = createShaderModule(readFile(config.vertSpv));

    vk::ShaderModule fragShaderModule{};
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
    shaderStages.push_back({{}, vk::ShaderStageFlagBits::eVertex, vertShaderModule, "main"});
    if (!config.fragSpv.empty()) {
        fragShaderModule = createShaderModule(readFile(config.fragSpv));
        shaderStages.push_back({{}, vk::ShaderStageFlagBits::eFragment, fragShaderModule, "main"});
    }

    // Vertex input — empty for fullscreen triangle passes
    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo();
    if (config.hasVertexInput) {
        vertexInputInfo.setVertexBindingDescriptions(bindingDescription)
                       .setVertexAttributeDescriptions(attributeDescriptions);
    }

    auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo()
                         .setTopology(vk::PrimitiveTopology::eTriangleList)
                         .setPrimitiveRestartEnable(false);

    auto viewportState = vk::PipelineViewportStateCreateInfo().setViewportCount(1).setScissorCount(1);

    auto rasterizer = vk::PipelineRasterizationStateCreateInfo()
                      .setDepthClampEnable(false)
                      .setRasterizerDiscardEnable(false)
                      .setPolygonMode(vk::PolygonMode::eFill)
                      .setLineWidth(1.0f)
                      .setCullMode(config.cullMode)
                      .setFrontFace(vk::FrontFace::eCounterClockwise)
                      .setDepthBiasEnable(config.depthBiasEnable);

    auto multisampling = vk::PipelineMultisampleStateCreateInfo()
                         .setSampleShadingEnable(false)
                         .setRasterizationSamples(vk::SampleCountFlagBits::e1);

    auto depthStencil = vk::PipelineDepthStencilStateCreateInfo()
                        .setDepthTestEnable(config.depthTestEnable)
                        .setDepthWriteEnable(config.depthWriteEnable)
                        .setDepthCompareOp(vk::CompareOp::eLess)
                        .setDepthBoundsTestEnable(false)
                        .setStencilTestEnable(false);

    // One blend attachment per MRT color output — all with blending disabled
    std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments(config.colorFormats.size());
    for (auto &att : colorBlendAttachments) {
        att.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
           .setBlendEnable(false);
    }

    auto colorBlending = vk::PipelineColorBlendStateCreateInfo()
                         .setLogicOpEnable(false)
                         .setAttachments(colorBlendAttachments);

    // Dynamic rendering — color format(s) + optional depth
    vk::PipelineRenderingCreateInfo pipelineRenderingInfo;
    pipelineRenderingInfo.setColorAttachmentFormats(config.colorFormats);
    if (config.depthTestEnable || config.depthWriteEnable) {
        pipelineRenderingInfo.setDepthAttachmentFormat(config.depthFormat);
    }

    std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    if (config.depthBiasEnable)
        dynamicStates.push_back(vk::DynamicState::eDepthBias);
    auto dynamicStateInfo = vk::PipelineDynamicStateCreateInfo({}, dynamicStates);

    auto pipelineInfo = vk::GraphicsPipelineCreateInfo()
                        .setPNext(&pipelineRenderingInfo)
                        .setStages(shaderStages)
                        .setPVertexInputState(&vertexInputInfo)
                        .setPInputAssemblyState(&inputAssembly)
                        .setPViewportState(&viewportState)
                        .setPRasterizationState(&rasterizer)
                        .setPMultisampleState(&multisampling)
                        .setPDepthStencilState(&depthStencil)
                        .setPColorBlendState(&colorBlending)
                        .setPDynamicState(&dynamicStateInfo)
                        .setLayout(layout);

    auto result = context_.getDevice().createGraphicsPipeline(nullptr, pipelineInfo);
    if (result.result != vk::Result::eSuccess)
        throw std::runtime_error("failed to create graphics pipeline: " + config.vertSpv);

    if (fragShaderModule) context_.getDevice().destroyShaderModule(fragShaderModule);
    context_.getDevice().destroyShaderModule(vertShaderModule);

    return result.value;
}

vk::ShaderModule GraphicsPipeline::createShaderModule(const std::vector<char> &code) const {
    auto createInfo =
        vk::ShaderModuleCreateInfo().setCodeSize(code.size()).setPCode(reinterpret_cast<const uint32_t *>(code.data()));

    return context_.getDevice().createShaderModule(createInfo);
}

void GraphicsPipeline::createPipelineLayout(const std::vector<vk::DescriptorSetLayout> &desLayouts) {
    auto pushConstantRange = vk::PushConstantRange()
                             .setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
                             .setOffset(0)
                             .setSize(sizeof(MeshPushConstants));

    const auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
                                    .setSetLayouts(desLayouts)
                                    .setPushConstantRanges(pushConstantRange);

    pipelineLayout_ = context_.getDevice().createPipelineLayout(pipelineLayoutInfo);
}

void GraphicsPipeline::createSsaoBlurPipelineLayout(vk::DescriptorSetLayout blurLayout) {
    // Blur pipeline uses a single descriptor set (set = 0) with depth + raw SSAO.
    // No push constants needed.
    const auto layoutInfo = vk::PipelineLayoutCreateInfo().setSetLayouts(blurLayout);
    ssaoBlurPipelineLayout_ = context_.getDevice().createPipelineLayout(layoutInfo);
}

void GraphicsPipeline::createDirShadowPipelineLayout() {
    // Shadow pass: vertex-only push constants, no descriptor sets
    auto pushConstantRange = vk::PushConstantRange()
                             .setStageFlags(vk::ShaderStageFlagBits::eVertex)
                             .setOffset(0)
                             .setSize(sizeof(DirShadowDataConstants));

    const auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
                                    .setPushConstantRanges(pushConstantRange);

    dirShadowPipelineLayout_ = context_.getDevice().createPipelineLayout(pipelineLayoutInfo);
}