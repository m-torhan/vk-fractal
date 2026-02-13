/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <vulkan/vulkan.h>

#include <stdexcept>
#include <string>

inline void vk_check(VkResult r, const char *what) {
    if (r != VK_SUCCESS) {
        throw std::runtime_error(std::string(what) + " (VkResult=" + std::to_string(r) + ")");
    }
}
