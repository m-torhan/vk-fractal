/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VKF_FIELD_INTERFACE_GLSL
#define VKF_FIELD_INTERFACE_GLSL

struct FieldSample {
    float d;   // distance bound / step hint
    float aux; // optional: trap/iter/density
};

FieldSample field_eval(vec3 p);

#endif /* VKF_FIELD_INTERFACE_GLSL */
