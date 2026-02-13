/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx/camera.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

void Camera::processMouse(float dx, float dy) {
    const float yaw_rad = -dx * mouse_sensitivity;
    const float pitch_rad = +dy * mouse_sensitivity;

    // Local axes expressed in world space
    const glm::vec3 up_axis = getUp();
    const glm::vec3 right_axis = getRight();

    const glm::quat q_yaw = glm::angleAxis(yaw_rad, up_axis);
    const glm::quat q_pitch = glm::angleAxis(pitch_rad, right_axis);

    // Apply increments (world-space rotations about current local axes)
    orientation = glm::normalize(q_yaw * q_pitch * orientation);
}

void Camera::processKeyboard(
    bool forward, bool backward, bool left, bool right, bool roll_left, bool roll_right, float dt) {
    const float v = move_speed * dt;

    const glm::vec3 fw = getForward();
    const glm::vec3 rt = getRight();

    if (forward) {
        position += fw * v;
    }
    if (backward) {
        position -= fw * v;
    }
    if (right) {
        position += rt * v;
    }
    if (left) {
        position -= rt * v;
    }

    // Roll around LOCAL forward
    const float roll_speed = glm::radians(120.0f); // deg/s
    float dr = 0.0f;
    if (roll_left) {
        dr += roll_speed * dt;
    }
    if (roll_right) {
        dr -= roll_speed * dt;
    }

    if (dr != 0.0f) {
        const glm::quat q_roll = glm::angleAxis(dr, fw);
        orientation = glm::normalize(q_roll * orientation);
    }
}

glm::mat4 Camera::getViewMatrix() const {
    // View = inverse(world transform). For rigid transform: inverse(R)*inverse(T)
    const glm::mat4 R = glm::toMat4(orientation);
    const glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
    return glm::inverse(T * R);
}

void Camera::getBasis(glm::vec3 &forward, glm::vec3 &right, glm::vec3 &up) const {
    forward = getForward();
    right = getRight();
    up = getUp();
}
