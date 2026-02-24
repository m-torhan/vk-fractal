/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

class ImGuiLayer {
public:
    void init(GLFWwindow *window,
              VkInstance instance,
              VkPhysicalDevice phys,
              VkDevice device,
              uint32_t queue_family,
              VkQueue queue,
              VkRenderPass render_pass,
              uint32_t image_count,
              VkCommandPool upload_cmd_pool);

    void new_frame();
    void render(VkCommandBuffer cmd);
    void shutdown();

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkDescriptorPool descriptor_pool_{VK_NULL_HANDLE};
};
