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
  float dr = 1.0;   // derivative magnitude accumulator
  float r = 0.0;
  int i = 0;

  for (i = 0; i < iterations; i++) {
    r = length(z);
    if (r > bailout) {
      break;
    }

    // Convert to spherical coordinates
    float theta = acos(clamp(z.z / max(r, 1e-8), -1.0, 1.0));
    float phi   = atan(z.y, z.x);

    // Scale and rotate the point
    float zr = pow(r, power);
    float theta_p = theta * power;
    float phi_p   = phi * power;

    // Derivative estimate for distance estimator
    dr = pow(r, power - 1.0) * power * dr + 1.0;

    // Back to cartesian
    z = zr * vec3(
      sin(theta_p) * cos(phi_p),
      sin(theta_p) * sin(phi_p),
      cos(theta_p)
    ) + p;
  }

  // Distance estimator
  float dist = 0.5 * log(r) * r / dr;

  // aux can be used for coloring (e.g., r or iterations-ish)
  return FieldSample(dist, i);
}
