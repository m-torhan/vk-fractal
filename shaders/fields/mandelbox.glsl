/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "field_interface.glsl"

float globalScale = 1.0f / 6.0f;

// --- Mandelbox helpers ---

// Box fold: reflect coordinates into [-foldLimit, foldLimit]
vec3 box_fold(vec3 z, float foldLimit) {
    return clamp(z, -foldLimit, foldLimit) * 2.0 - z;
}

// Sphere fold: radial inversion between minRadius and fixedRadius
// Updates z and the derivative scale factor drScale.
vec3 sphere_fold(vec3 z, float minRadius, float fixedRadius, inout float drScale) {
    float r2 = dot(z, z);
    float minR2 = minRadius * minRadius;
    float fixR2 = fixedRadius * fixedRadius;

    if (r2 < minR2) {
        // Scale up small radii
        float t = fixR2 / minR2;
        z *= t;
        drScale *= t;
    } else if (r2 < fixR2) {
        // Invert inside the sphere of radius fixedRadius
        float t = fixR2 / r2;
        z *= t;
        drScale *= t;
    }
    return z;
}

// Mandelbox distance estimator (DE).
FieldSample field_mandelbox(vec3 p, int iterations, float bailout) {
    const float scale = 2.0;
    const float foldLimit = 1.0;
    const float minRadius = 0.5;
    const float fixedRadius = 1.0;

    p /= globalScale; // scale space

    vec3 z = p;
    float dr = 1.0;    // derivative magnitude accumulator
    float trap = 1e20; // orbit trap for coloring (min radius)

    for (int i = 0; i < iterations; ++i) {
        // Box fold
        z = box_fold(z, foldLimit);

        // Sphere fold (updates derivative scaling)
        float drScale = 1.0;
        z = sphere_fold(z, minRadius, fixedRadius, drScale);
        dr *= drScale;

        // Scale + translate
        z = z * scale + p;
        dr = dr * abs(scale) + 1.0;

        // Orbit trap: minimum radius reached
        trap = min(trap, length(z));

        // Bailout
        if (dot(z, z) > bailout * bailout)
            break;
    }

    // Distance estimator
    float r = length(z);
    float d = r / abs(dr);

    d *= globalScale;

    return FieldSample(d, trap);
}
