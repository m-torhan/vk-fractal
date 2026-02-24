/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "field_interface.glsl"

// Params:
//  - p: position
//  - c: julia constant (controls shape)
//  - iterations: 10..20
//  - power: 8 typical
//  - bailout: 2..16 (8 is good)
FieldSample field_julia(vec3 p, vec3 c, int iterations, float power, float bailout) {
    vec3 z = p;
    float dr = 1.0;
    float r = 0.0;

    int i = 0;
    for (i = 0; i < iterations; ++i) {
        r = length(z);
        if (r > bailout)
            break;

        float r_safe = max(r, 1e-8);

        float theta = acos(clamp(z.z / r_safe, -1.0, 1.0));
        float phi = atan(z.y, z.x);

        float rp1 = pow(r_safe, power - 1.0);
        float zr = rp1 * r_safe;

        dr = rp1 * power * dr + 1.0;

        float theta_p = theta * power;
        float phi_p = phi * power;

        z = zr * vec3(sin(theta_p) * cos(phi_p), sin(theta_p) * sin(phi_p), cos(theta_p)) + c;
    }

    float r_safe = max(r, 1e-6);
    float dr_safe = max(abs(dr), 1e-6);

    float dist = 0.5 * log(r_safe) * r_safe / dr_safe;

    // Smooth-ish aux for coloring
    float aux = float(i) / float(max(iterations, 1));
    if (r_safe > 1.0) {
        float smooth_i = float(i) - log2(max(log(r_safe), 1e-6)) / max(log2(power), 1e-6);
        aux = clamp(smooth_i / float(max(iterations, 1)), 0.0, 1.0);
    }

    return FieldSample(dist, aux);
}
