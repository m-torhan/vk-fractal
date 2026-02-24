/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <vulkan/vulkan.h>

#include <vector>

class VkContext;

class Swapchain {
public:
    void init(VkContext &ctx, uint32_t w, uint32_t h);
    void shutdown(VkDevice device);
    void recreate(VkContext &ctx, uint32_t w, uint32_t h);

    VkSwapchainKHR handle() const { return swapchain_; }
    VkFormat format() const { return format_; }
    VkExtent2D extent() const { return extent_; }

    uint32_t image_count() const { return static_cast<uint32_t>(images_.size()); }
    VkRenderPass render_pass() const { return render_pass_; }

    const std::vector<VkImageView> &image_views() const { return views_; }

private:
    void create(VkContext &ctx, uint32_t w, uint32_t h);
    void destroy(VkDevice device);

    void create_render_pass(VkDevice device);
    void destroy_render_pass(VkDevice device);

    VkSwapchainKHR swapchain_{};

    VkFormat format_{};
    VkExtent2D extent_{};
    std::vector<VkImage> images_;
    std::vector<VkImageView> views_;

    VkRenderPass render_pass_{VK_NULL_HANDLE};
};
