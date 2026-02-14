#include "graphics_pipeline.hpp"
#include "swap_chain.hpp"
#include "VulkanContext.hpp"
#include "renderer/Vertex.hpp"
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
}

GraphicsPipeline::~GraphicsPipeline() {
    std::cerr << "[Destructor] GraphicsPipeline starting..." << std::endl;
    auto device = context_.getDevice();
    device.destroyPipeline(graphicsPipeline_);
    std::cerr << "[Destructor] GraphicsPipeline-graphicsPipeline_..." << std::endl;
    device.destroyPipelineLayout(pipelineLayout_);
    std::cerr << "[Destructor] GraphicsPipeline-pipelineLayout_..." << std::endl;

}

void GraphicsPipeline::createGraphicsPipeline() {
    auto vertShaderCode = readFile("shaders/blinn-phong/blinn-phong.vert.spv");
    auto fragShaderCode = readFile("shaders/blinn-phong/blinn-phong.frag.spv");

    vk::ShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    vk::ShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    // Shader Stages
    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vertShaderModule, "main"),
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, fragShaderModule, "main")
    };

    // Vertex Input
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo()
                           .setVertexBindingDescriptions(bindingDescription)
                           .setVertexAttributeDescriptions(attributeDescriptions);

    // Input Assembly
    auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo()
                         .setTopology(vk::PrimitiveTopology::eTriangleList)
                         .setPrimitiveRestartEnable(false);

    // Viewport State (Dynamic, so count only)
    auto viewportState = vk::PipelineViewportStateCreateInfo()
                         .setViewportCount(1)
                         .setScissorCount(1);

    // Rasterizer
    auto rasterizer = vk::PipelineRasterizationStateCreateInfo()
                      .setDepthClampEnable(false)
                      .setRasterizerDiscardEnable(false)
                      .setPolygonMode(vk::PolygonMode::eFill)
                      .setLineWidth(1.0f)
                      .setCullMode(vk::CullModeFlagBits::eBack)
                      .setFrontFace(vk::FrontFace::eCounterClockwise)
                      .setDepthBiasEnable(false);

    // Multisampling
    auto multisampling = vk::PipelineMultisampleStateCreateInfo()
                         .setSampleShadingEnable(false)
                         .setRasterizationSamples(vk::SampleCountFlagBits::e1);

    // Depth/Stencil
    auto depthStencil = vk::PipelineDepthStencilStateCreateInfo()
                        .setDepthTestEnable(true)
                        .setDepthWriteEnable(true)
                        .setDepthCompareOp(vk::CompareOp::eLess)
                        .setDepthBoundsTestEnable(false)
                        .setStencilTestEnable(false);

    // Color Blending
    auto colorBlendAttachment = vk::PipelineColorBlendAttachmentState()
                                .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                   vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
                                .setBlendEnable(false);

    auto colorBlending = vk::PipelineColorBlendStateCreateInfo()
                         .setLogicOpEnable(false)
                         .setAttachments(colorBlendAttachment);

    // Dynamic State
    std::array<vk::DynamicState, 2> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    auto dynamicStateInfo = vk::PipelineDynamicStateCreateInfo({}, dynamicStates);

    // Create Pipeline
    auto pipelineInfo = vk::GraphicsPipelineCreateInfo()
                        .setStages(shaderStages)
                        .setPVertexInputState(&vertexInputInfo)
                        .setPInputAssemblyState(&inputAssembly)
                        .setPViewportState(&viewportState)
                        .setPRasterizationState(&rasterizer)
                        .setPMultisampleState(&multisampling)
                        .setPDepthStencilState(&depthStencil)
                        .setPColorBlendState(&colorBlending)
                        .setPDynamicState(&dynamicStateInfo)
                        .setLayout(pipelineLayout_)
                        .setRenderPass(renderPass_)
                        .setSubpass(0);

    auto result = context_.getDevice().createGraphicsPipeline(nullptr, pipelineInfo);
    if (result.result != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
    graphicsPipeline_ = result.value;

    // Shader modules can be destroyed immediately after pipeline creation
    context_.getDevice().destroyShaderModule(fragShaderModule);
    context_.getDevice().destroyShaderModule(vertShaderModule);
}

vk::ShaderModule GraphicsPipeline::createShaderModule(const std::vector<char> &code) const {
    auto createInfo = vk::ShaderModuleCreateInfo()
                      .setCodeSize(code.size())
                      .setPCode(reinterpret_cast<const uint32_t *>(code.data()));

    return context_.getDevice().createShaderModule(createInfo);
}

void GraphicsPipeline::createPipelineLayout(vk::DescriptorSetLayout dsLayout) {
    // Push Constant for shading mode
    auto pushConstantRange = vk::PushConstantRange()
                             .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                             .setOffset(0)
                             .setSize(sizeof(int));

    auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
                              .setSetLayouts(dsLayout)
                              .setPushConstantRanges(pushConstantRange);

    pipelineLayout_ = context_.getDevice().createPipelineLayout(pipelineLayoutInfo);
}