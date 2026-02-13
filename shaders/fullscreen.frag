/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#version 460

#include "field_interface.glsl"
#include "fields/mandelbulb.glsl"
#include "fields/mandelbox.glsl"

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_color;

// Keep the UBO std140-friendly: use vec4/ivec4 groups.
layout(std140, set = 0, binding = 0) uniform Params {
  vec4 cam_pos;     // xyz: position
  vec4 cam_fw;      // xyz: forward
  vec4 cam_rt;      // xyz: right
  vec4 cam_up;      // xyz: up

  vec4 render0;     // x=max_dist, y=hit_eps, z=normal_eps, w=fov_scale
  ivec4 render1;    // x=max_steps, y=field_id, z=iterations, w=debug_flags (unused)

  vec4 fractal0;    // x=bailout, y=power, z,w unused
  vec4 misc0;       // x=time, y=aspect, z,w unused
} U;

float sdf_sphere(vec3 p, float r) {
  return length(p) - r;
}

float sdf_box(vec3 p, vec3 b) {
  vec3 q = abs(p) - b;
  return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

FieldSample field_eval(vec3 p) {
  int id = U.render1.y;

  if (id == 0) {
    // sphere debug
    float d = sdf_sphere(p, 1.0f);
    return FieldSample(d, 0.0);
  } else if (id == 1) {
    // box debug
    float d = sdf_box(p, vec3(1.0));
    return FieldSample(d, 0.0);
  } else if (id == 2) {
    int   iters   = max(U.render1.z, 1);
    float bailout = max(U.fractal0.x, 2.0);
    float power   = max(U.fractal0.y, 2.0);
    return field_mandelbulb(p, iters, power, bailout);
  }
 else if (id == 3) {
    return field_mandelbox(p);
  }

  return FieldSample(1e9, 0.0);
}

vec3 estimate_normal(vec3 p) {
  float e = max(U.render0.z, 1e-5);
  float dx = field_eval(p + vec3(e,0,0)).d - field_eval(p - vec3(e,0,0)).d;
  float dy = field_eval(p + vec3(0,e,0)).d - field_eval(p - vec3(0,e,0)).d;
  float dz = field_eval(p + vec3(0,0,e)).d - field_eval(p - vec3(0,0,e)).d;
  return normalize(vec3(dx,dy,dz));
}

void main() {
  // Always-visible background
  vec2 uv01 = v_uv;                // should already be 0..1
  vec2 xy   = uv01 * 2.0 - 1.0;    // -1..1

  // Background gradient
  vec3 bg   = vec3(0.08 + 0.35*uv01.x, 0.08 + 0.35*uv01.y, 0.20);

  // If UBO is clearly broken, show bright red
  if (any(isnan(U.cam_pos)) || any(isnan(U.cam_fw)) || U.render1.x <= 0) {
    o_color = vec4(1.0, 0.0, 0.0, 1.0);
    return;
  }

  // Aspect correction (expects CPU to write aspect = width/height into misc0.y)
  float aspect = (U.misc0.y > 0.0) ? U.misc0.y : 1.0;
  xy.x *= aspect;

  vec3 ro = U.cam_pos.xyz;

  // Use basis from CPU (supports roll)
  vec3 fw = U.cam_fw.xyz;
  vec3 rt = U.cam_rt.xyz;
  vec3 up = U.cam_up.xyz;

  // Normalize
  fw = normalize(fw);
  rt = normalize(rt);
  up = normalize(up);

  // Optional: re-orthonormalize to fight drift if CPU basis isn’t perfect
  // (Keeps rt perpendicular to fw, then recompute up)
  rt = normalize(rt - fw * dot(rt, fw));
  up = normalize(cross(rt, fw));

  float fov = (U.render0.w > 0.0) ? U.render0.w : 1.2; // default if unset

  vec3 rd = normalize(fw + xy.x * rt * fov + xy.y * up * fov);

  float max_dist   = max(U.render0.x, 0.01);
  float hit_eps    = max(U.render0.y, 1e-6);

  int max_steps_u = U.render1.x;
  const int MAX_STEPS_CAP = 2048;

  float t = 0.0;
  bool hit = false;
  float aux = 0.0;
  int steps = 0;

  for (int i = 0; i < MAX_STEPS_CAP; i++) {
    if (i >= max_steps_u) {
      break;
    }

    vec3 p = ro + t * rd;
    FieldSample s = field_eval(p);
    aux = s.aux;

    float d = s.d;
    if (isnan(d)) d = 1e-3;

    if (d < hit_eps) { hit = true; steps = i; break; }

    // Clamp step to avoid stalls / negative weirdness
    float step = clamp(d, 1e-4, 1.0);
    t += step;

    if (t > max_dist) break;
  }

  if (!hit) {
    // encode steps in blue a bit so you can see variation if any
    float s = float(steps) / float(max(max_steps_u, 1));
    o_color = vec4(bg.rg, clamp(bg.b + 0.5 * s, 0.0, 1.0), 1.0);
    return;
  }

  vec3 p = ro + t * rd;
  vec3 n = estimate_normal(p);

  vec3 l = normalize(vec3(0.6, 0.7, 0.2));
  float ndotl = max(dot(n, l), 0.0);

  float s = float(steps) / float(max(max_steps_u, 1));
  float c = clamp(aux * 0.25, 0.0, 1.0);

  vec3 base = mix(vec3(0.2, 0.3, 0.6), vec3(0.9, 0.8, 0.2), c);
  vec3 col = base * (0.15 + 0.85 * ndotl);

  // Put step count into red channel slightly (debug)
  col.r = clamp(col.r + 0.35 * s, 0.0, 1.0);

  o_color = vec4(col, 1.0);
}
