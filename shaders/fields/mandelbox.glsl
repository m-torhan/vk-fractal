/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "field_interface.glsl"

FieldSample field_mandelbox(vec3 p) {
  // Placeholder: box SDF, aux = max component
  vec3 q = abs(p) - vec3(1.0);
  float d = length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
  float aux = max(abs(p.x), max(abs(p.y), abs(p.z)));
  return FieldSample(d, aux);
}
