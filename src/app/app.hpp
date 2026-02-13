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

struct alignas(16) ParamsUBO {
    alignas(4) float time = 0.0f;
    alignas(4) float _pad_time[3] = {0, 0, 0};

    alignas(16) glm::vec4 cam_pos = {0, 0, 3, 0};
    alignas(16) glm::vec4 cam_dir = {0, 0, -1, 0};

    alignas(4) float max_dist = 100.0f;
    alignas(4) int max_steps = 256;
    alignas(4) float hit_eps = 1e-3f;
    alignas(4) float normal_eps = 1e-3f;

    alignas(4) int field_id = 0;
    alignas(4) int iterations = 12;
    alignas(4) float bailout = 8.0f;
    alignas(4) float power = 8.0f;

    alignas(16) glm::vec4 p0 = {0, 0, 0, 0};
    alignas(16) glm::vec4 p1 = {0, 0, 0, 0};
};

static_assert(sizeof(ParamsUBO) % 16 == 0);

class App {
public:
    void run();
    void on_framebuffer_resize(int width, int height);
    void on_mouse_move(double x, double y);
    void on_field_change(int d);

private:
    void init_window();
    void init_vulkan();
    void shutdown();

    void draw_frame(float time_seconds);
    void recreate_swapchain_if_needed();

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

    unsigned int field_id_ = 2; // 2 = (mandelbulb)
};
