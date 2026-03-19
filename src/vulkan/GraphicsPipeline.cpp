#include "GraphicsPipeline.hpp"
#include "VulkanContext.hpp"
#include "../common/Vertex.hpp"
#include "SwapChain.hpp"
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

GraphicsPipeline::~GraphicsPipeline() {
    auto device = context_.getDevice();
    device.destroyPipeline(pbrPipeline_);
    device.destroyPipeline(unlitPipeline_);
    device.destroyPipelineLayout(pipelineLayout_);
}

vk::Pipeline GraphicsPipeline::buildPipeline(const std::string &vertSpv, const std::string &fragSpv) const {
    auto vertShaderCode = readFile(vertSpv);
    auto fragShaderCode = readFile(fragSpv);

    vk::ShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    vk::ShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex,   vertShaderModule, "main"),
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, fragShaderModule, "main")};

    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo()
                           .setVertexBindingDescriptions(bindingDescription)
                           .setVertexAttributeDescriptions(attributeDescriptions);

    auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo()
                         .setTopology(vk::PrimitiveTopology::eTriangleList)
                         .setPrimitiveRestartEnable(false);

    auto viewportState = vk::PipelineViewportStateCreateInfo().setViewportCount(1).setScissorCount(1);

    auto rasterizer = vk::PipelineRasterizationStateCreateInfo()
                      .setDepthClampEnable(false)
                      .setRasterizerDiscardEnable(false)
                      .setPolygonMode(vk::PolygonMode::eFill)
                      .setLineWidth(1.0f)
                      .setCullMode(vk::CullModeFlagBits::eBack)
                      .setFrontFace(vk::FrontFace::eCounterClockwise)
                      .setDepthBiasEnable(false);

    auto multisampling = vk::PipelineMultisampleStateCreateInfo()
                         .setSampleShadingEnable(false)
                         .setRasterizationSamples(vk::SampleCountFlagBits::e1);

    auto depthStencil = vk::PipelineDepthStencilStateCreateInfo()
                        .setDepthTestEnable(true)
                        .setDepthWriteEnable(true)
                        .setDepthCompareOp(vk::CompareOp::eLess)
                        .setDepthBoundsTestEnable(false)
                        .setStencilTestEnable(false);

    auto colorBlendAttachment = vk::PipelineColorBlendAttachmentState()
                                .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                   vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
                                .setBlendEnable(false);

    auto colorBlending = vk::PipelineColorBlendStateCreateInfo()
                         .setLogicOpEnable(false)
                         .setAttachments(colorBlendAttachment);

    vk::Format colorFormat = swapChain_.getColorFormat();
    vk::Format depthFormat = vk::Format::eD32Sfloat;

    vk::PipelineRenderingCreateInfo pipelineRenderingInfo;
    pipelineRenderingInfo.setColorAttachmentFormats(colorFormat)
                         .setDepthAttachmentFormat(depthFormat);

    std::array<vk::DynamicState, 2> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
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
                        .setLayout(pipelineLayout_);

    auto result = context_.getDevice().createGraphicsPipeline(nullptr, pipelineInfo);
    if (result.result != vk::Result::eSuccess)
        throw std::runtime_error("failed to create graphics pipeline: " + vertSpv);

    context_.getDevice().destroyShaderModule(fragShaderModule);
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
