/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include "gfx/vk_bootstrap.hpp"

class VkContext {
public:
    void init(GLFWwindow *window);
    void shutdown();

    VkInstance instance() const { return instance_; }
    VkPhysicalDevice phys() const { return phys_; }
    VkDevice device() const { return device_; }
    VkSurfaceKHR surface() const { return surface_; }

    VkQueue graphics_queue() const { return graphics_queue_; }
    VkQueue present_queue() const { return present_queue_; }
    uint32_t graphics_qf() const { return qf_.graphics; }
    uint32_t present_qf() const { return qf_.present; }

    VkCommandPool command_pool() const { return cmd_pool_; }

    VkPhysicalDeviceProperties properties() const { return props_; }

private:
    QueueFamilyIndices find_queue_families(VkPhysicalDevice dev);
    bool is_device_suitable(VkPhysicalDevice dev);

    VkInstance instance_{};
    VkPhysicalDevice phys_{};
    VkDevice device_{};
    VkSurfaceKHR surface_{};

    VkQueue graphics_queue_{};
    VkQueue present_queue_{};
    QueueFamilyIndices qf_{};

    VkCommandPool cmd_pool_{};

    VkPhysicalDeviceProperties props_{};
};
