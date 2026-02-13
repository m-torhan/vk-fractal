/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>
#include <stdexcept>

#include "gfx/frame_resources.hpp"
#include "gfx/vk_context.hpp"
#include "util/checks.hpp"

static uint32_t find_mem_type(VkPhysicalDevice phys, uint32_t type_bits, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
        if ((type_bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    throw std::runtime_error("No suitable memory type");
}

static void make_buffer(VkContext &ctx,
                        VkDeviceSize size,
                        VkBufferUsageFlags usage,
                        VkMemoryPropertyFlags mem_flags,
                        VkBuffer &buf,
                        VkDeviceMemory &mem) {
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vk_check(vkCreateBuffer(ctx.device(), &bci, nullptr, &buf), "vkCreateBuffer");

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(ctx.device(), buf, &req);

    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = find_mem_type(ctx.phys(), req.memoryTypeBits, mem_flags);

    vk_check(vkAllocateMemory(ctx.device(), &mai, nullptr, &mem), "vkAllocateMemory");
    vk_check(vkBindBufferMemory(ctx.device(), buf, mem, 0), "vkBindBufferMemory");
}

void FrameRing::init(VkContext &ctx) {
    // Allocate command buffers
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = ctx.command_pool();
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = kMaxFrames;

    VkCommandBuffer cbs[kMaxFrames]{};
    vk_check(vkAllocateCommandBuffers(ctx.device(), &cai, cbs), "vkAllocateCommandBuffers");

    for (uint32_t i = 0; i < kMaxFrames; i++) {
        frames_[i].cmd = cbs[i];

        VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        vk_check(vkCreateSemaphore(ctx.device(), &sci, nullptr, &frames_[i].image_acquired),
                 "vkCreateSemaphore(image_acquired)");
        vk_check(vkCreateSemaphore(ctx.device(), &sci, nullptr, &frames_[i].render_finished),
                 "vkCreateSemaphore(render_finished)");

        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vk_check(vkCreateFence(ctx.device(), &fci, nullptr, &frames_[i].in_flight), "vkCreateFence");

        // Uniform buffer (host visible)
        make_buffer(ctx,
                    512,
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    frames_[i].ubo,
                    frames_[i].ubo_mem);

        vk_check(vkMapMemory(ctx.device(), frames_[i].ubo_mem, 0, VK_WHOLE_SIZE, 0, &frames_[i].ubo_mapped),
                 "vkMapMemory(ubo)");
    }
}

void FrameRing::shutdown(VkDevice device) {
    for (auto &f : frames_) {
        if (f.ubo_mapped) {
            vkUnmapMemory(device, f.ubo_mem);
        }
        if (f.ubo) {
            vkDestroyBuffer(device, f.ubo, nullptr);
        }
        if (f.ubo_mem) {
            vkFreeMemory(device, f.ubo_mem, nullptr);
        }

        if (f.image_acquired) {
            vkDestroySemaphore(device, f.image_acquired, nullptr);
        }
        if (f.render_finished) {
            vkDestroySemaphore(device, f.render_finished, nullptr);
        }
        if (f.in_flight) {
            vkDestroyFence(device, f.in_flight, nullptr);
        }
    }
}
