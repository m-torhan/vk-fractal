/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx/fullscreen_pipeline.hpp"

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#include "gfx/swapchain.hpp"
#include "gfx/vk_context.hpp"
#include "util/checks.hpp"
#include "util/read_file.hpp"

VkShaderModule FullscreenPipeline::load_shader(VkDevice device, const std::string &path) {
    auto bytes = read_file_binary(path);
    if (bytes.size() % 4 != 0) {
        throw std::runtime_error("SPIR-V size not multiple of 4: " + path);
    }

    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = bytes.size();
    ci.pCode = reinterpret_cast<const uint32_t *>(bytes.data());

    VkShaderModule mod{};
    vk_check(vkCreateShaderModule(device, &ci, nullptr, &mod), "vkCreateShaderModule");
    return mod;
}

void FullscreenPipeline::init(VkContext &ctx, const Swapchain &sw, const std::string &shader_dir) {
    // ----------------------------
    // Render pass (single color)
    // ----------------------------
    VkAttachmentDescription color{};
    color.format = sw.format();
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

    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &color_ref;

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
    rpci.pSubpasses = &sub;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dep;

    vk_check(vkCreateRenderPass(ctx.device(), &rpci, nullptr, &rp_), "vkCreateRenderPass");

    // ----------------------------
    // Descriptor set layout (UBO at set=0, binding=0)
    // ----------------------------
    VkDescriptorSetLayoutBinding b0{};
    b0.binding = 0;
    b0.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    b0.descriptorCount = 1;
    b0.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dslci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 1;
    dslci.pBindings = &b0;
    vk_check(vkCreateDescriptorSetLayout(ctx.device(), &dslci, nullptr, &dsl_), "vkCreateDescriptorSetLayout");

    // Descriptor pool for 2 frames (2 sets)
    VkDescriptorPoolSize ps{};
    ps.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ps.descriptorCount = 2;

    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 2;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &ps;
    vk_check(vkCreateDescriptorPool(ctx.device(), &dpci, nullptr, &dspool_), "vkCreateDescriptorPool");

    // Allocate descriptor sets
    std::array<VkDescriptorSetLayout, 2> layouts = {dsl_, dsl_};
    VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = dspool_;
    dsai.descriptorSetCount = 2;
    dsai.pSetLayouts = layouts.data();
    vk_check(vkAllocateDescriptorSets(ctx.device(), &dsai, ds_), "vkAllocateDescriptorSets");

    // Pipeline layout
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &dsl_;
    vk_check(vkCreatePipelineLayout(ctx.device(), &plci, nullptr, &layout_), "vkCreatePipelineLayout");

    // ----------------------------
    // Shaders
    // ----------------------------
    VkShaderModule vs = load_shader(ctx.device(), shader_dir + "/fullscreen.vert.spv");
    VkShaderModule fs = load_shader(ctx.device(), shader_dir + "/fullscreen.frag.spv");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName = "main";

    // ----------------------------
    // Fixed-function: fullscreen triangle, no vertex buffers
    // ----------------------------
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // IMPORTANT: make viewport/scissor dynamic so resize works
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    ds.dynamicStateCount = 2;
    ds.pDynamicStates = dyn_states;

    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    VkGraphicsPipelineCreateInfo gpci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gpci.stageCount = 2;
    gpci.pStages = stages;
    gpci.pVertexInputState = &vi;
    gpci.pInputAssemblyState = &ia;
    gpci.pViewportState = &vp;
    gpci.pRasterizationState = &rs;
    gpci.pMultisampleState = &ms;
    gpci.pColorBlendState = &cb;
    gpci.pDynamicState = &ds;
    gpci.layout = layout_;
    gpci.renderPass = rp_;
    gpci.subpass = 0;

    vk_check(vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1, &gpci, nullptr, &pipe_),
             "vkCreateGraphicsPipelines");

    vkDestroyShaderModule(ctx.device(), fs, nullptr);
    vkDestroyShaderModule(ctx.device(), vs, nullptr);

    recreate_framebuffers(ctx.device(), sw);
}

void FullscreenPipeline::recreate_framebuffers(VkDevice device, const Swapchain &sw) {
    for (auto f : fb_) {
        vkDestroyFramebuffer(device, f, nullptr);
    }
    fb_.clear();

    fb_.resize(sw.image_views().size());

    for (size_t i = 0; i < sw.image_views().size(); i++) {
        VkImageView att[] = {sw.image_views()[i]};

        VkFramebufferCreateInfo fci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = rp_;
        fci.attachmentCount = 1;
        fci.pAttachments = att;
        fci.width = sw.extent().width;
        fci.height = sw.extent().height;
        fci.layers = 1;

        vk_check(vkCreateFramebuffer(device, &fci, nullptr, &fb_[i]), "vkCreateFramebuffer");
    }
}

void FullscreenPipeline::shutdown(VkDevice device) {
    for (auto f : fb_) {
        vkDestroyFramebuffer(device, f, nullptr);
    }
    fb_.clear();

    if (pipe_) {
        vkDestroyPipeline(device, pipe_, nullptr);
    }
    if (layout_) {
        vkDestroyPipelineLayout(device, layout_, nullptr);
    }
    if (dspool_) {
        vkDestroyDescriptorPool(device, dspool_, nullptr);
    }
    if (dsl_) {
        vkDestroyDescriptorSetLayout(device, dsl_, nullptr);
    }
    if (rp_) {
        vkDestroyRenderPass(device, rp_, nullptr);
    }

    pipe_ = VK_NULL_HANDLE;
    layout_ = VK_NULL_HANDLE;
    dspool_ = VK_NULL_HANDLE;
    dsl_ = VK_NULL_HANDLE;
    rp_ = VK_NULL_HANDLE;
}
