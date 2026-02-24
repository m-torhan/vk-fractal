/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <GLFW/glfw3.h>
#include <cstdint>

#include <glm/glm.hpp>

#include "gfx/camera.hpp"
#include "gfx/frame_resources.hpp"
#include "gfx/fullscreen_pipeline.hpp"
#include "gfx/swapchain.hpp"
#include "gfx/vk_context.hpp"

struct alignas(16) GpuParams {
    float cam_pos[4] = {0, 0, 3, 0};

    // Camera basis (supports roll):
    // fw = forward, rt = right, up = up
    float cam_fw[4] = {0, 0, -1, 0};
    float cam_rt[4] = {1, 0, 0, 0};
    float cam_up[4] = {0, 1, 0, 0};

    float render0[4] = {100.0f, 1e-3f, 1e-3f, 1.2f}; // max_dist, hit_eps, normal_eps, fov
    int render1[4] = {256, 0, 12, 0};                // max_steps, field_id, iterations, debug_flags

    float fractal0[4] = {8.0f, 8.0f, 0.0f, 0.0f}; // bailout, power, ...
    float misc0[4] = {0.0f, 1.0f, 0.0f, 0.0f};    // time, aspect, ...
};
static_assert(sizeof(GpuParams) % 16 == 0);

class App {
public:
    void run();
    void on_framebuffer_resize(int width, int height);
    void on_mouse_move(double x, double y);
    void on_field_change(int d);
    bool mouse_locked() { return mouse_locked_; }
    void toggle_mouse_lock() { mouse_locked_ = !mouse_locked_; }

private:
    void init_window();
    void init_vulkan();
    void shutdown();

    void draw_frame(float time_seconds);
    void recreate_swapchain_if_needed();

    void build_ui();

    GLFWwindow *window_{};
    uint32_t win_w_ = 1280;
    uint32_t win_h_ = 720;
    bool framebuffer_resized_ = false;

    VkContext ctx_;
    Swapchain sw_;
    FullscreenPipeline fsq_;
    FrameRing frames_;

    VkClearValue clear_{};

    float cam_yaw_ = 0.0f;
    float cam_pitch_ = 0.0f;
    float cam_radius_ = 4.0f;
    glm::vec3 cam_target_ = {0, 0, 0};

    Camera camera_;
    bool first_mouse_ = true;
    double last_x_, last_y_;

    GpuParams params_{};

    bool mouse_locked_ = true;
};
