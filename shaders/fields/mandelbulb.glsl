/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "field_interface.glsl"

// Mandelbulb distance estimator.
// Params:
//  - iterations: typical 8..20
//  - power: typical 8
//  - bailout: typical 2..8
FieldSample field_mandelbulb(vec3 p, int iterations, float power, float bailout) {
    vec3 z = p;
    float dr = 1.0;
    float r = 0.0;
    int i = 0;

    for (i = 0; i < iterations; ++i) {
        r = length(z);
        if (r > bailout) {
            break;
        }

        float r_safe = max(r, 1e-8);

        // Convert to spherical coordinates
        float theta = acos(clamp(z.z / r_safe, -1.0, 1.0));
        float phi = atan(z.y, z.x);

        // Scale and rotate the point
        float rp1 = pow(r_safe, power - 1.0);
        float zr = rp1 * r_safe;

        // Derivative
        dr = rp1 * power * dr + 1.0;

        float theta_p = theta * power;
        float phi_p = phi * power;

        z = zr * vec3(sin(theta_p) * cos(phi_p), sin(theta_p) * sin(phi_p), cos(theta_p)) + p;
    }

    // Robust DE tail
    float r_safe = max(r, 1e-6);
    float dr_safe = max(abs(dr), 1e-6);

    // Distance estimator
    float dist = 0.5 * log(r_safe) * r_safe / dr_safe;

    // Smooth-ish iteration metric for coloring
    float aux = float(i) / float(max(iterations, 1));
    if (r_safe > 1.0) {
        // classic smooth iteration formula for escape-time fractals
        float smooth_i = float(i) - log2(max(log(r_safe), 1e-6)) / max(log2(power), 1e-6);
        aux = clamp(smooth_i / float(max(iterations, 1)), 0.0, 1.0);
    }

    return FieldSample(dist, aux);
}
