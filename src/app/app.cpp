/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <imgui.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/app.hpp"
#include "util/checks.hpp"

namespace {

std::string shader_dir_from_exe() {
    std::vector<char> buf(4096);
    ssize_t n = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if (n > 0) {
        buf[n] = '\0';
        std::filesystem::path exe_path(buf.data());
        return (exe_path.parent_path() / "shaders").string();
    }
    return (std::filesystem::current_path() / "shaders").string();
}

void framebuffer_resize_cb(GLFWwindow *w, int width, int height) {
    auto *app = reinterpret_cast<App *>(glfwGetWindowUserPointer(w));
    if (app) {
        app->on_framebuffer_resize(width, height);
    }
}

static void cursor_pos_cb(GLFWwindow *w, double x, double y) {
    auto *app = reinterpret_cast<App *>(glfwGetWindowUserPointer(w));
    if (app) {
        app->on_mouse_move(x, y);
    }
}

static void key_cb(GLFWwindow *w, int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;

    auto *app = reinterpret_cast<App *>(glfwGetWindowUserPointer(w));

    if (app && action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_Z:
            app->on_field_change(-1);
            break;
        case GLFW_KEY_X:
            app->on_field_change(1);
            break;
        case GLFW_KEY_C:
            if (app->mouse_locked()) {
                glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            app->toggle_mouse_lock();
            break;
        }
    }
}

} // namespace

void App::on_mouse_move(double xpos, double ypos) {
    if (!mouse_locked_) {
        return;
    }

    if (first_mouse_) {
        last_x_ = xpos;
        last_y_ = ypos;
        first_mouse_ = false;
    }

    float dx = xpos - last_x_;
    float dy = ypos - last_y_;

    last_x_ = xpos;
    last_y_ = ypos;

    camera_.processMouse(dx, dy);
}

void App::on_framebuffer_resize(int width, int height) {
    framebuffer_resized_ = true;
    if (width > 0) {
        win_w_ = static_cast<uint32_t>(width);
    }
    if (height > 0) {
        win_h_ = static_cast<uint32_t>(height);
    }
}

void App::on_field_change(int d) { params_.render1[1] += d; }

void App::init_window() {
    if (!glfwInit()) {
        throw std::runtime_error("glfwInit failed");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window_ = glfwCreateWindow(static_cast<int>(win_w_), static_cast<int>(win_h_), "vk-fractal", nullptr, nullptr);
    if (!window_) {
        throw std::runtime_error("glfwCreateWindow failed");
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebuffer_resize_cb);
    glfwSetCursorPosCallback(window_, cursor_pos_cb);
    glfwSetKeyCallback(window_, key_cb);
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    clear_.color = {{0.15f, 0.15f, 0.18f, 1.0f}};
}

void App::init_vulkan() {
    params_.render1[1] = 2;      // field_id
    params_.render1[2] = 256;    // iterations
    params_.fractal0[0] = 32.0f; // bailout
    params_.fractal0[1] = 8.0f;  // power
    params_.render0[0] = 50.0f;  // max_dist (mandelbulb is “dense”)
    params_.render1[0] = 256;    // max_steps
    params_.render0[1] = 1e-3f;  // hit_eps

    ctx_.init(window_);

    int fb_w = 0, fb_h = 0;
    glfwGetFramebufferSize(window_, &fb_w, &fb_h);
    if (fb_w <= 0 || fb_h <= 0) {
        fb_w = static_cast<int>(win_w_);
        fb_h = static_cast<int>(win_h_);
    }

    sw_.init(ctx_, static_cast<uint32_t>(fb_w), static_cast<uint32_t>(fb_h));
    frames_.init(ctx_);

    const std::string shader_dir = shader_dir_from_exe();
    fsq_.init(ctx_, sw_, shader_dir);

    ctx_.init_imgui(window_, sw_.render_pass(), sw_.image_count());

    // Update each descriptor set to point at the matching frame's UBO.
    VkBuffer ubos[FrameRing::kMaxFrames]{};

    for (uint32_t i = 0; i < FrameRing::kMaxFrames; i++) {
        ubos[i] = frames_.current().ubo;
        frames_.advance();
    }
    // Now we are back at original frame index.

    for (uint32_t i = 0; i < FrameRing::kMaxFrames; i++) {
        VkDescriptorBufferInfo bi{};
        bi.buffer = ubos[i];
        bi.offset = 0;
        bi.range = sizeof(GpuParams);

        VkWriteDescriptorSet wds{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wds.dstSet = fsq_.ds(i);
        wds.dstBinding = 0;
        wds.descriptorCount = 1;
        wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wds.pBufferInfo = &bi;

        vkUpdateDescriptorSets(ctx_.device(), 1, &wds, 0, nullptr);
    }
}

void App::shutdown() {
    if (ctx_.device()) {
        vkDeviceWaitIdle(ctx_.device());
        fsq_.shutdown(ctx_.device());
        frames_.shutdown(ctx_.device());
        sw_.shutdown(ctx_.device());
        ctx_.shutdown();
    }

    if (window_) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

void App::recreate_swapchain_if_needed() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    if (w <= 0 || h <= 0) {
        return; // minimized
    }

    vkDeviceWaitIdle(ctx_.device());
    sw_.recreate(ctx_, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    fsq_.recreate_framebuffers(ctx_.device(), sw_);

    framebuffer_resized_ = false;
}

void App::draw_frame(float time_seconds) {
    auto &f = frames_.current();

    vk_check(vkWaitForFences(ctx_.device(), 1, &f.in_flight, VK_TRUE, UINT64_MAX), "vkWaitForFences");
    vk_check(vkResetFences(ctx_.device(), 1, &f.in_flight), "vkResetFences");

    uint32_t img_idx = 0;
    VkResult acq =
        vkAcquireNextImageKHR(ctx_.device(), sw_.handle(), UINT64_MAX, f.image_acquired, VK_NULL_HANDLE, &img_idx);

    if (acq == VK_ERROR_OUT_OF_DATE_KHR || acq == VK_SUBOPTIMAL_KHR) {
        recreate_swapchain_if_needed();
        return;
    }
    vk_check(acq, "vkAcquireNextImageKHR");

    // --- ImGui ---
    ctx_.imgui_new_frame();
    build_ui();

    // --- Update UBO ---
    params_.misc0[0] = time_seconds;
    params_.misc0[1] = static_cast<float>(sw_.extent().width) / static_cast<float>(sw_.extent().height); // aspect

    glm::vec3 pos;
    pos.x = cam_radius_ * cos(cam_pitch_) * sin(cam_yaw_);
    pos.y = cam_radius_ * sin(cam_pitch_);
    pos.z = cam_radius_ * cos(cam_pitch_) * cos(cam_yaw_);

    pos += cam_target_;

    params_.cam_pos[0] = pos.x;
    params_.cam_pos[1] = pos.y;
    params_.cam_pos[2] = pos.z;

    glm::vec3 fw, rt, up;
    camera_.getBasis(fw, rt, up);

    params_.cam_pos[0] = camera_.position.x;
    params_.cam_pos[1] = camera_.position.y;
    params_.cam_pos[2] = camera_.position.z;

    params_.cam_fw[0] = fw.x;
    params_.cam_fw[1] = fw.y;
    params_.cam_fw[2] = fw.z;

    params_.cam_rt[0] = rt.x;
    params_.cam_rt[1] = rt.y;
    params_.cam_rt[2] = rt.z;

    params_.cam_up[0] = up.x;
    params_.cam_up[1] = up.y;
    params_.cam_up[2] = up.z;

    std::memcpy(f.ubo_mapped, &params_, sizeof(params_));

    // --- Record command buffer ---
    vk_check(vkResetCommandBuffer(f.cmd, 0), "vkResetCommandBuffer");

    VkCommandBufferBeginInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vk_check(vkBeginCommandBuffer(f.cmd, &cbi), "vkBeginCommandBuffer");

    VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderPass = fsq_.render_pass();
    rpbi.framebuffer = fsq_.framebuffer(img_idx);
    rpbi.renderArea.offset = {0, 0};
    rpbi.renderArea.extent = sw_.extent();
    rpbi.clearValueCount = 1;
    rpbi.pClearValues = &clear_;

    vkCmdBeginRenderPass(f.cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fsq_.pipeline());

    // Dynamic viewport/scissor (CRITICAL for resize correctness)
    VkViewport vp{};
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.width = static_cast<float>(sw_.extent().width);
    vp.height = static_cast<float>(sw_.extent().height);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(f.cmd, 0, 1, &vp);

    VkRect2D sc{};
    sc.offset = {0, 0};
    sc.extent = sw_.extent();
    vkCmdSetScissor(f.cmd, 0, 1, &sc);

    // Bind descriptor set matching this frame-in-flight
    VkDescriptorSet ds = fsq_.ds(frames_.index());
    vkCmdBindDescriptorSets(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fsq_.layout(), 0, 1, &ds, 0, nullptr);

    vkCmdDraw(f.cmd, 3, 1, 0, 0);

    ctx_.imgui_render(f.cmd);

    vkCmdEndRenderPass(f.cmd);

    vk_check(vkEndCommandBuffer(f.cmd), "vkEndCommandBuffer");

    // --- Submit ---
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &f.image_acquired;
    si.pWaitDstStageMask = &wait_stage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &f.cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &f.render_finished;

    vk_check(vkQueueSubmit(ctx_.graphics_queue(), 1, &si, f.in_flight), "vkQueueSubmit");

    // --- Present ---
    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &f.render_finished;

    VkSwapchainKHR sc_handle = sw_.handle();
    pi.swapchainCount = 1;
    pi.pSwapchains = &sc_handle;
    pi.pImageIndices = &img_idx;

    VkResult pr = vkQueuePresentKHR(ctx_.present_queue(), &pi);
    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR || framebuffer_resized_) {
        recreate_swapchain_if_needed();
    } else {
        vk_check(pr, "vkQueuePresentKHR");
    }

    frames_.advance();
}

void App::build_ui() {
    ImGui::Begin("Fractal Controls");

    ImGui::Text("Camera");
    ImGui::DragFloat3("Position", &camera_.position.x, 0.01f);

    ImGui::Separator();

    ImGui::Text("Raymarch");
    ImGui::SliderInt("Max steps", &params_.render1[0], 16, 2048);
    ImGui::SliderFloat("Max dist", &params_.render0[0], 1e-3f, 10.0f, "%.6f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Hit eps", &params_.render0[1], 1e-6f, 1e-2f, "%.6f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Normal eps", &params_.render0[4], 1e-6f, 1e-2f, "%.6f", ImGuiSliderFlags_Logarithmic);

    ImGui::Separator();

    ImGui::Text("Fractal");

    const char *fields[] = {"Sphere", "Box", "Mandelbulb", "Mandelbox"};

    ImGui::Combo("Field", &params_.render1[1], fields, IM_ARRAYSIZE(fields));

    ImGui::SliderInt("Iterations", &params_.render1[2], 16, 2048);
    ImGui::SliderFloat("Power", &params_.fractal0[1], 2.0f, 32.0f);
    ImGui::SliderFloat("Bailout", &params_.fractal0[0], 1.0f, 200.0f);

    ImGui::End();
}

void App::run() {
    init_window();
    init_vulkan();

    auto t0 = std::chrono::high_resolution_clock::now();
    auto last_t1 = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        auto t1 = std::chrono::high_resolution_clock::now();
        float sec = std::chrono::duration<float>(t1 - t0).count();
        float dt = std::chrono::duration<float>(t1 - last_t1).count();
        last_t1 = t1;
        camera_.processKeyboard(glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS,
                                glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS,
                                glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS,
                                glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS,
                                glfwGetKey(window_, GLFW_KEY_Q) == GLFW_PRESS,
                                glfwGetKey(window_, GLFW_KEY_E) == GLFW_PRESS,
                                dt);

        draw_frame(sec);
    }

    shutdown();
}
