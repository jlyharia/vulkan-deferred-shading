//
// Created by johnny on 3/28/26.
//

#include "SsaoPass.hpp"

#include "PassUtils.hpp"
#include "common/Config.hpp"
#include "vulkan/GraphicsPipeline.hpp"
#include "vulkan/SwapChain.hpp"
#include "vulkan/VulkanUtils.hpp"

#include <random>


SsaoPass::SsaoPass(SwapChain &swapChain, VulkanContext &context) : swapChain_(swapChain), context_(context) {
    generateSSAOKernel(SSAO_KERNEL_SIZE);
    generateSsaoNoise();
    createImages(swapChain.getExtent().width, swapChain.getExtent().height);
}

SsaoPass::~SsaoPass() { cleanup(); }

void SsaoPass::execute(vk::CommandBuffer cmd,
                       const GraphicsPipeline &pipeline,
                       vk::DescriptorSet globalDescSet,
                       vk::DescriptorSet gbufferDescSet,
                       vk::DescriptorSet ssaoDescSet) const {
    auto extent = swapChain_.getExtent();
    const vk::PipelineLayout layout = pipeline.getPipelineLayout();

    auto colorAttachment = pass_util::colorAttachment(ssaoBuffer_.view,
                                                       vk::AttachmentLoadOp::eDontCare);

    vk::RenderingInfo renderingInfo{};
    renderingInfo.setRenderArea({{0, 0}, extent})
                 .setLayerCount(1)
                 .setColorAttachments(colorAttachment);

    cmd.beginRendering(renderingInfo);
    {
        pass_util::setViewportScissor(cmd, extent);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getSsaoPipeline());

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               layout, DescriptorSets::GLOBAL_SET,
                               {globalDescSet}, {});

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               layout, DescriptorSets::GBUFFER_SET,
                               {gbufferDescSet}, {});

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               layout, DescriptorSets::SSAO_SET,
                               {ssaoDescSet}, {});

        cmd.draw(3, 1, 0, 0); // fullscreen triangle — no vertex buffer
    }
    cmd.endRendering();

    // Pipeline barrier: SSAO buffer → shader-read for blur pass
    auto barrier = vk_util::colorAttachmentToShaderRead(ssaoBuffer_.image);
    cmd.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(barrier));
}

/**
 * Generate SSAO noise texture
 * Generate 4x4 random noise texture
 */
void SsaoPass::generateSsaoNoise() {
    // RG16F: only XY components used (rotation vector in tangent space).
    // GLSL samples as vec4(r, g, 0, 1) — the zero z is correct.
    std::vector<glm::vec2> ssaoNoise;
    for (unsigned int i = 0; i < 16; i++) {
        ssaoNoise.push_back({generateRandomFloat() * 2.0f - 1.0f,
                             generateRandomFloat() * 2.0f - 1.0f});
    }
    vk_util::uploadToDeviceImage(
        context_.getVmaAllocator(), context_.getDevice(),
        context_.getTransferCommandPool(), context_.getGraphicsQueue(),
        ssaoNoise.data(), sizeof(glm::vec2) * 16,
        4, 4, SSAO_NOISE_FORMAT,
        ssaoNoiseImage_, ssaoNoiseAllocation_);
    ssaoNoiseImageView_ = vk_util::createImageView(context_.getDevice(), ssaoNoiseImage_, SSAO_NOISE_FORMAT);

}

/**
 *Generate SSAO kernel
 * @param kernelSize should be multiple of 16
 */
void SsaoPass::generateSSAOKernel(size_t kernelSize) {

    // (kernelSize + 15) & ~15 is a fast bitwise way to do this
    kernelSize = (kernelSize + 15) & ~15;

    // Safety check: ensure it's at least 16
    if (kernelSize == 0)
        kernelSize = 16;
    std::vector<glm::vec4> ssaoKernel_;
    ssaoKernel_.reserve(kernelSize);
    for (size_t i = 0; i < kernelSize; ++i) {
        glm::vec3 sample(
            generateRandomFloat() * 2.0f - 1.0f, // x: [-1, 1]
            generateRandomFloat() * 2.0f - 1.0f, // y: [-1, 1]
            generateRandomFloat() // z: [0, 1]
            );
        sample = glm::normalize(sample); // now the sample is on the surface of hemisphere
        const float scale = static_cast<float>(i) / static_cast<float>(kernelSize); // // solve gap and clumps
        sample *= glm::mix(0.1f, 1.0f, scale * scale); // Bias towards center, accelerating interpolation function
        ssaoKernel_.push_back(glm::vec4(sample, 0.0f));
    }
    // todo refactor uploadToDeviceBuffer, there are other places duplicate similar logic

    vk_util::uploadToDeviceBuffer(
        context_.getVmaAllocator(),
        context_.getDevice(),
        context_.getGraphicsQueue(),
        context_.getTransferCommandPool(),
        ssaoKernel_,
        vk::BufferUsageFlagBits::eUniformBuffer,
        kernelBuffer_,
        kernelBufferAlloc_);

}

static std::mt19937 gen(std::random_device{}());
static std::uniform_real_distribution dis(0.0f, 1.0f);

float SsaoPass::generateRandomFloat() {
    return dis(gen);
}

void SsaoPass::createImages(uint32_t width, uint32_t height) {
    constexpr auto usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    ssaoBuffer_ = vk_util::AttachmentImage::create(context_.getVmaAllocator(), context_.getDevice(),
                                                    width, height, SSAO_BUFFER_FORMAT, usage);
}

void SsaoPass::cleanup() {
    auto device    = context_.getDevice();
    auto allocator = context_.getVmaAllocator();

    ssaoBuffer_.cleanup(device, allocator);

    if (ssaoNoiseImageView_)
        device.destroyImageView(ssaoNoiseImageView_);
    if (ssaoNoiseImage_)
        vmaDestroyImage(allocator, ssaoNoiseImage_, ssaoNoiseAllocation_);

    if (kernelBuffer_)
        vmaDestroyBuffer(allocator, kernelBuffer_, kernelBufferAlloc_);
}