//
// Created by johnny on 3/28/26.
//

#include "SsaoPass.hpp"

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

    // for ssao pass render target
    auto colorAttachment = vk::RenderingAttachmentInfo()
                           .setImageView(ssaoBufferImageView_)
                           .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                           .setLoadOp(vk::AttachmentLoadOp::eDontCare)
                           .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingInfo renderingInfo{};
    renderingInfo.setRenderArea({{0, 0}, extent})
                 .setLayerCount(1)
                 .setColorAttachments(colorAttachment);

    cmd.beginRendering(renderingInfo);
    {
        cmd.setViewport(0, vk::Viewport(0.0f, 0.0f,
                                        static_cast<float>(extent.width),
                                        static_cast<float>(extent.height),
                                        0.0f, 1.0f));
        cmd.setScissor(0, vk::Rect2D({0, 0}, extent));

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
    // todo refactor barrier code to put in a free function with namespace
    // Pipeline barrier: ssao buffer → shader-read for lighting pass
    auto makeColorBarrier = [](vk::Image image) {
        return vk::ImageMemoryBarrier2()
               .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
               .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
               .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
               .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
               .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
               .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
               .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
               .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
               .setImage(image)
               .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
    };

    vk::ImageMemoryBarrier2 barriers = makeColorBarrier(this->ssaoBufferImage_);

    vk::DependencyInfo depInfo;
    depInfo.setImageMemoryBarriers(barriers);

    cmd.pipelineBarrier2(depInfo);
}

/**
 * Generate SSAO noise texture
 * Generate 4x4 random noise texture
 */
void SsaoPass::generateSsaoNoise() {
    std::vector<glm::vec4> ssaoNoise;
    for (unsigned int i = 0; i < 16; i++) {
        glm::vec4 noise(generateRandomFloat() * 2.0f - 1.0f,
                        generateRandomFloat() * 2.0f - 1.0f,
                        0.0f,
                        0.0f // unused
            );
        // rotate around z-axis (in tangent space)
        ssaoNoise.push_back(noise);
    }
    const auto allocator = context_.getVmaAllocator();
    const auto device = context_.getDevice();
    const auto queue = context_.getGraphicsQueue();
    const auto commandPool = context_.getTransferCommandPool();
    const vk::DeviceSize bufferSize = sizeof(glm::vec4) * 16;

    // staging buffer
    vk::Buffer stagingBuffer;
    VmaAllocation stagingAlloc;
    vk_util::createBuffer(allocator, bufferSize,
                          vk::BufferUsageFlagBits::eTransferSrc,
                          VMA_MEMORY_USAGE_CPU_ONLY,
                          stagingBuffer, stagingAlloc);
    void *mapped;
    vmaMapMemory(allocator, stagingAlloc, &mapped);
    memcpy(mapped, ssaoNoise.data(), bufferSize);
    vmaUnmapMemory(allocator, stagingAlloc);

    // create image
    vk_util::createImage(allocator, 4, 4, SSAO_NOISE_FORMAT,
                         vk::ImageTiling::eOptimal,
                         vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                         VMA_MEMORY_USAGE_GPU_ONLY,
                         ssaoNoiseImage_, ssaoNoiseAllocation_);

    // upload
    vk_util::transitionImageLayout(device, commandPool, queue,
                                   ssaoNoiseImage_, SSAO_NOISE_FORMAT,
                                   vk::ImageLayout::eUndefined,
                                   vk::ImageLayout::eTransferDstOptimal);
    vk_util::copyBufferToImage(device, commandPool, queue,
                               stagingBuffer, ssaoNoiseImage_, 4, 4);
    vk_util::transitionImageLayout(device, commandPool, queue,
                                   ssaoNoiseImage_, SSAO_NOISE_FORMAT,
                                   vk::ImageLayout::eTransferDstOptimal,
                                   vk::ImageLayout::eShaderReadOnlyOptimal);

    ssaoNoiseImageView_ = vk_util::createImageView(device, ssaoNoiseImage_, SSAO_NOISE_FORMAT);
    vmaDestroyBuffer(allocator, stagingBuffer, stagingAlloc);

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
    const auto device = context_.getDevice();
    const auto allocator = context_.getVmaAllocator();

    // RT0: occlusion factor — 1 byte/pixel
    vk_util::createImage(allocator, width, height,
                         SSAO_BUFFER_FORMAT,
                         vk::ImageTiling::eOptimal,
                         usage,
                         VMA_MEMORY_USAGE_GPU_ONLY,
                         ssaoBufferImage_,
                         ssaoBufferAlloc_);
    ssaoBufferImageView_ = vk_util::createImageView(device, ssaoBufferImage_, SSAO_BUFFER_FORMAT);
}

void SsaoPass::cleanup() {
    auto device = context_.getDevice();
    auto allocator = context_.getVmaAllocator();

    if (ssaoBufferImageView_)
        device.destroyImageView(ssaoBufferImageView_);
    if (ssaoBufferImage_)
        vmaDestroyImage(allocator, ssaoBufferImage_, ssaoBufferAlloc_);

    if (ssaoNoiseImageView_)
        device.destroyImageView(ssaoNoiseImageView_);
    if (ssaoNoiseImage_)
        vmaDestroyImage(allocator, ssaoNoiseImage_, ssaoNoiseAllocation_);

    if (kernelBuffer_)
        vmaDestroyBuffer(allocator, kernelBuffer_, kernelBufferAlloc_);
}