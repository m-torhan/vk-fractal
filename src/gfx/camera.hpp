/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

class Camera {
public:
    glm::vec3 position{0.0f, 0.0f, 3.0f};

    float move_speed = 0.5f;

    // Radians per pixel
    float mouse_sensitivity = 0.0025f;

    // Authoritative orientation
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};

    glm::mat4 getViewMatrix() const;

    // Mouse: dx -> yaw around LOCAL up, dy -> pitch around LOCAL right
    void processMouse(float dx, float dy);

    // Keyboard: WASD move in local frame; "up/down" params are roll left/right (Q/E)
    void processKeyboard(bool forward, bool backward, bool left, bool right, bool roll_left, bool roll_right, float dt);

    void getBasis(glm::vec3 &forward, glm::vec3 &right, glm::vec3 &up) const;

private:
    glm::vec3 getForward() const { return glm::normalize(orientation * glm::vec3(0, 0, -1)); }
    glm::vec3 getRight() const { return glm::normalize(orientation * glm::vec3(1, 0, 0)); }
    glm::vec3 getUp() const { return glm::normalize(orientation * glm::vec3(0, 1, 0)); }
};
