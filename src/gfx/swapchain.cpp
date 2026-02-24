/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx/swapchain.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "gfx/vk_context.hpp"
#include "util/checks.hpp"

static VkSurfaceFormatKHR choose_format(const std::vector<VkSurfaceFormatKHR> &fmts) {
    for (auto &f : fmts) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return fmts.at(0);
}

static VkPresentModeKHR choose_present(const std::vector<VkPresentModeKHR> &modes) {
    for (auto m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
            return m;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR &caps, uint32_t w, uint32_t h) {
    if (caps.currentExtent.width != UINT32_MAX) {
        return caps.currentExtent;
    }
    VkExtent2D e{};
    e.width = std::clamp(w, caps.minImageExtent.width, caps.maxImageExtent.width);
    e.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    return e;
}

void Swapchain::create(VkContext &ctx, uint32_t w, uint32_t h) {
    VkSurfaceCapabilitiesKHR caps{};
    vk_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.phys(), ctx.surface(), &caps),
             "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    uint32_t fmt_count = 0;
    vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.phys(), ctx.surface(), &fmt_count, nullptr),
             "vkGetPhysicalDeviceSurfaceFormatsKHR(count)");
    std::vector<VkSurfaceFormatKHR> fmts(fmt_count);
    vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.phys(), ctx.surface(), &fmt_count, fmts.data()),
             "vkGetPhysicalDeviceSurfaceFormatsKHR(list)");

    uint32_t pm_count = 0;
    vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.phys(), ctx.surface(), &pm_count, nullptr),
             "vkGetPhysicalDeviceSurfacePresentModesKHR(count)");
    std::vector<VkPresentModeKHR> pms(pm_count);
    vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.phys(), ctx.surface(), &pm_count, pms.data()),
             "vkGetPhysicalDeviceSurfacePresentModesKHR(list)");

    auto chosen_fmt = choose_format(fmts);
    auto chosen_pm = choose_present(pms);
    extent_ = choose_extent(caps, w, h);
    format_ = chosen_fmt.format;

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = ctx.surface();
    ci.minImageCount = image_count;
    ci.imageFormat = chosen_fmt.format;
    ci.imageColorSpace = chosen_fmt.colorSpace;
    ci.imageExtent = extent_;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = chosen_pm;
    ci.clipped = VK_TRUE;

    uint32_t qfs[] = {ctx.graphics_qf(), ctx.present_qf()};
    if (ctx.graphics_qf() != ctx.present_qf()) {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = qfs;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    vk_check(vkCreateSwapchainKHR(ctx.device(), &ci, nullptr, &swapchain_), "vkCreateSwapchainKHR");

    uint32_t img_count = 0;
    vk_check(vkGetSwapchainImagesKHR(ctx.device(), swapchain_, &img_count, nullptr), "vkGetSwapchainImagesKHR(count)");
    images_.resize(img_count);
    vk_check(vkGetSwapchainImagesKHR(ctx.device(), swapchain_, &img_count, images_.data()),
             "vkGetSwapchainImagesKHR(list)");

    views_.resize(img_count);
    for (uint32_t i = 0; i < img_count; i++) {
        VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image = images_[i];
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = format_;
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.layerCount = 1;
        vk_check(vkCreateImageView(ctx.device(), &vci, nullptr, &views_[i]), "vkCreateImageView");
    }

    create_render_pass(ctx.device());
}

void Swapchain::destroy(VkDevice device) {
    destroy_render_pass(device);

    for (auto v : views_) {
        vkDestroyImageView(device, v, nullptr);
    }
    views_.clear();
    images_.clear();
    if (swapchain_) {
        vkDestroySwapchainKHR(device, swapchain_, nullptr);
    }
    swapchain_ = VK_NULL_HANDLE;
}

void Swapchain::init(VkContext &ctx, uint32_t w, uint32_t h) { create(ctx, w, h); }

void Swapchain::shutdown(VkDevice device) { destroy(device); }

void Swapchain::recreate(VkContext &ctx, uint32_t w, uint32_t h) {
    destroy(ctx.device());
    create(ctx, w, h);
}

void Swapchain::create_render_pass(VkDevice device) {
    VkAttachmentDescription color{};
    color.format = format_;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    // Basic dependency for layout transition / sync
    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpci.attachmentCount = 1;
    rpci.pAttachments = &color;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dep;

    vk_check(vkCreateRenderPass(device, &rpci, nullptr, &render_pass_), "vkCreateRenderPass");
}

void Swapchain::destroy_render_pass(VkDevice device) {
    if (render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, render_pass_, nullptr);
        render_pass_ = VK_NULL_HANDLE;
    }
}
