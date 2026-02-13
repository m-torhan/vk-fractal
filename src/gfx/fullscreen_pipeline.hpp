/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

class VkContext;
class Swapchain;

class FullscreenPipeline {
public:
    void init(VkContext &ctx, const Swapchain &sw, const std::string &shader_dir);
    void shutdown(VkDevice device);

    void recreate_framebuffers(VkDevice device, const Swapchain &sw);

    VkRenderPass render_pass() const { return rp_; }
    VkPipelineLayout layout() const { return layout_; }
    VkPipeline pipeline() const { return pipe_; }

    VkDescriptorSetLayout dsl() const { return dsl_; }
    VkDescriptorPool dspool() const { return dspool_; }
    VkDescriptorSet ds(uint32_t frame_index) const { return ds_[frame_index]; }

    VkFramebuffer framebuffer(uint32_t swap_img) const { return fb_.at(swap_img); }

private:
    VkShaderModule load_shader(VkDevice device, const std::string &path);

    VkRenderPass rp_{};
    VkDescriptorSetLayout dsl_{};
    VkDescriptorPool dspool_{};
    VkPipelineLayout layout_{};
    VkPipeline pipe_{};

    std::vector<VkFramebuffer> fb_;
    VkDescriptorSet ds_[2]{};
};
