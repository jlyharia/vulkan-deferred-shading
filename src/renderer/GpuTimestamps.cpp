#include "GpuTimestamps.hpp"

#include <cassert>

GpuTimestamps::GpuTimestamps(vk::Device device, vk::PhysicalDevice physicalDevice)
    : device_(device),
      timestampPeriodNs_(physicalDevice.getProperties().limits.timestampPeriod)
{
    const auto info = vk::QueryPoolCreateInfo{}
        .setQueryType(vk::QueryType::eTimestamp)
        .setQueryCount(kMaxPasses * 2);
    for (uint32_t i = 0; i < engineConfig::MAX_FRAMES_IN_FLIGHT; ++i)
        pools_[i] = device_.createQueryPool(info);
}

GpuTimestamps::~GpuTimestamps() {
    for (auto &p : pools_)
        if (p) device_.destroyQueryPool(p);
}

void GpuTimestamps::resetPool(uint32_t frameIndex) {
    device_.resetQueryPool(pools_[frameIndex], 0, kMaxPasses * 2);
}

void GpuTimestamps::readback(uint32_t frameIndex,
                              const std::vector<std::string> &passNames,
                              uint32_t passCount)
{
    assert(passCount <= kMaxPasses);
    std::vector<uint64_t> raw(passCount * 2, 0);
    (void)device_.getQueryPoolResults(
        pools_[frameIndex], 0, passCount * 2,
        passCount * 2 * sizeof(uint64_t), raw.data(),
        sizeof(uint64_t),
        vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);

    results_.clear();
    for (uint32_t i = 0; i < passCount; ++i) {
        const float ms = static_cast<float>(raw[2 * i + 1] - raw[2 * i]) * timestampPeriodNs_ * 1e-6f;
        results_.push_back({passNames[i], ms});
    }
}

void GpuTimestamps::writeBegin(vk::CommandBuffer cmd, uint32_t frameIndex, uint32_t passIndex) const {
    cmd.writeTimestamp2(vk::PipelineStageFlagBits2::eTopOfPipe, pools_[frameIndex], 2 * passIndex);
}

void GpuTimestamps::writeEnd(vk::CommandBuffer cmd, uint32_t frameIndex, uint32_t passIndex) const {
    cmd.writeTimestamp2(vk::PipelineStageFlagBits2::eBottomOfPipe, pools_[frameIndex], 2 * passIndex + 1);
}
