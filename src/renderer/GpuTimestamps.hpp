#pragma once
#include "common/VulkanInclude.hpp"
#include "common/Config.hpp"
#include <array>
#include <string>
#include <vector>

// Owns one VkQueryPool per frame-in-flight slot for GPU timestamp queries.
// Each pool holds kMaxPasses * 2 slots: slot 2*i = pass begin, 2*i+1 = pass end.
// Usage pattern per frame:
//   1. readback()   — after fence wait, reads results from this slot's prior submission
//   2. resetPool()  — host-side pool reset (Vulkan 1.2), must follow fence wait
//   3. writeBegin/writeEnd — inside the command buffer, around each pass
class GpuTimestamps {
public:
    static constexpr uint32_t kMaxPasses = 8;

    struct Entry { std::string name; float gpuMs; };

    GpuTimestamps(vk::Device device, vk::PhysicalDevice physicalDevice);
    ~GpuTimestamps();
    GpuTimestamps(const GpuTimestamps &) = delete;
    GpuTimestamps &operator=(const GpuTimestamps &) = delete;

    // Reads back timestamps written by the previous submission of this frame slot.
    // Must be called after the frame fence has been waited on.
    void readback(uint32_t frameIndex, const std::vector<std::string> &passNames, uint32_t passCount);

    // Host-side pool reset — clears all query slots so writeBegin/writeEnd are valid.
    // Must be called after the fence wait and before command buffer recording.
    void resetPool(uint32_t frameIndex);

    // Write begin/end timestamps inside a command buffer.
    void writeBegin(vk::CommandBuffer cmd, uint32_t frameIndex, uint32_t passIndex) const;
    void writeEnd  (vk::CommandBuffer cmd, uint32_t frameIndex, uint32_t passIndex) const;

    [[nodiscard]] const std::vector<Entry> &results() const { return results_; }

private:
    vk::Device device_;
    float timestampPeriodNs_;
    std::array<vk::QueryPool, engineConfig::MAX_FRAMES_IN_FLIGHT> pools_{};
    std::vector<Entry> results_;
};
