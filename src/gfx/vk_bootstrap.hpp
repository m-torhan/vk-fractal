/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <vulkan/vulkan.h>

struct QueueFamilyIndices {
    uint32_t graphics = UINT32_MAX;
    uint32_t present = UINT32_MAX;
    bool complete() const { return graphics != UINT32_MAX && present != UINT32_MAX; }
};
