/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <imgui.h>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <stdexcept>
#include <vector>

#include "gfx/imgui_layer.hpp"
#include "util/checks.hpp"

namespace {

// Create a big descriptor pool that ImGui can use internally.
VkDescriptorPool create_imgui_descriptor_pool(VkDevice device) {
    const VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
    };

    VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    info.maxSets = 1000u * static_cast<uint32_t>(std::size(pool_sizes));
    info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    info.pPoolSizes = pool_sizes;

    VkDescriptorPool pool = VK_NULL_HANDLE;
    vk_check(vkCreateDescriptorPool(device, &info, nullptr, &pool), "vkCreateDescriptorPool");
    return pool;
}

} // namespace

void ImGuiLayer::init(GLFWwindow *window,
                      VkInstance instance,
                      VkPhysicalDevice phys,
                      VkDevice device,
                      uint32_t queue_family,
                      VkQueue queue,
                      VkRenderPass render_pass,
                      uint32_t image_count,
                      VkCommandPool /*upload_cmd_pool*/) {
    device_ = device;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    descriptor_pool_ = create_imgui_descriptor_pool(device_);
    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.ApiVersion = VK_API_VERSION_1_1;
    init_info.Instance = instance;
    init_info.PhysicalDevice = phys;
    init_info.Device = device_;
    init_info.QueueFamily = queue_family;
    init_info.Queue = queue;

    init_info.DescriptorPool = descriptor_pool_;
    init_info.DescriptorPoolSize = 0;

    init_info.MinImageCount = image_count;
    init_info.ImageCount = image_count;

    init_info.PipelineInfoMain.RenderPass = render_pass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    init_info.UseDynamicRendering = false;

    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    init_info.MinAllocationSize = 0;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        throw std::runtime_error("ImGui_ImplVulkan_Init failed");
    }
}

void ImGuiLayer::new_frame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::render(VkCommandBuffer cmd) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void ImGuiLayer::shutdown() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
    }

    device_ = VK_NULL_HANDLE;
}
