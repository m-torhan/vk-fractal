/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

struct FrameResources {
    VkCommandBuffer cmd{};
    VkSemaphore image_acquired{};
    VkSemaphore render_finished{};
    VkFence in_flight{};

    VkBuffer ubo{};
    VkDeviceMemory ubo_mem{};
    void *ubo_mapped{};
};

class VkContext;

class FrameRing {
public:
    static constexpr uint32_t kMaxFrames = 2;

    void init(VkContext &ctx);
    void shutdown(VkDevice device);

    FrameResources &frame(uint32_t i) { return frames_[i]; }
    const FrameResources &frame(uint32_t i) const { return frames_[i]; }

    FrameResources &current() { return frames_[frame_index_]; }
    uint32_t index() const { return frame_index_; }
    void advance() { frame_index_ = (frame_index_ + 1) % kMaxFrames; }

private:
    FrameResources frames_[kMaxFrames]{};
    uint32_t frame_index_ = 0;
};
