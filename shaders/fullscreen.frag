/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#version 460

#include "field_interface.glsl"
#include "fields/julia.glsl"
#include "fields/mandelbox.glsl"
#include "fields/mandelbulb.glsl"

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_color;

// Keep the UBO std140-friendly: use vec4/ivec4 groups.
layout(std140, set = 0, binding = 0) uniform Params {
    vec4 cam_pos; // xyz: position
    vec4 cam_fw;  // xyz: forward
    vec4 cam_rt;  // xyz: right
    vec4 cam_up;  // xyz: up

    vec4 render0;  // x=max_dist, y=hit_eps, z=normal_eps, w=fov_scale
    ivec4 render1; // x=max_steps, y=field_id, z=iterations, w=debug_flags (unused)

    vec4 fractal0; // x=bailout, y=power, z,w unused
    vec4 julia_c;  // x,y,z=Julia set constant, w unused
    vec4 misc0;    // x=time, y=aspect, z,w unused
}
U;

float sdf_sphere(vec3 p, float r) { return length(p) - r; }

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
        int iters = max(U.render1.z, 1);
        float bailout = max(U.fractal0.x, 2.0);
        float power = max(U.fractal0.y, 2.0);
        return field_mandelbulb(p, iters, power, bailout);
    } else if (id == 3) {
        int iters = max(U.render1.z, 1);
        float bailout = max(U.fractal0.x, 2.0);
        return field_mandelbox(p, iters, bailout);
    } else if (id == 4) {
        int iters = max(U.render1.z, 1);
        float bailout = max(U.fractal0.x, 2.0);
        float power = max(U.fractal0.y, 2.0);
        vec3 c = U.julia_c.xyz;
        return field_julia(p, c, iters, power, bailout);
    }

    return FieldSample(1e9, 0.0);
}

vec3 estimate_normal(vec3 p, float t) {
    float e0 = max(U.render0.z, 1e-5);
    float e = max(e0, 5e-4 * t);

    vec2 k = vec2(1.0, -1.0);
    float d1 = field_eval(p + k.xyy * e).d;
    float d2 = field_eval(p + k.yyx * e).d;
    float d3 = field_eval(p + k.yxy * e).d;
    float d4 = field_eval(p + k.xxx * e).d;

    vec3 n = k.xyy * d1 + k.yyx * d2 + k.yxy * d3 + k.xxx * d4;
    return normalize(n);
}

float ambient_occlusion(vec3 p, vec3 n) {
    float occ = 0.0;
    float sca = 1.0;

    for (int i = 1; i <= 5; ++i) {
        float h = 0.02 * float(i);
        float d = field_eval(p + n * h).d;
        occ += (h - d) * sca;
        sca *= 0.6;
    }
    return clamp(1.0 - 2.0 * occ, 0.0, 1.0);
}

void main() {
    // Always-visible background
    vec2 uv01 = v_uv;           // should already be 0..1
    vec2 xy = uv01 * 2.0 - 1.0; // -1..1

    // Background gradient
    vec3 bg = vec3(0.08 + 0.35 * uv01.x, 0.08 + 0.35 * uv01.y, 0.20);

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

    float max_dist = max(U.render0.x, 0.01);
    float hit_eps = max(U.render0.y, 1e-6);

    int max_steps_u = U.render1.x;
    const int MAX_STEPS_CAP = 2048;

    float t = 0.0;
    float t_prev = 0.0;
    bool hit = false;
    float aux = 0.0;
    int steps = 0;
    float step_scale = 0.75;

    for (int i = 0; i < MAX_STEPS_CAP; i++) {
        if (i >= max_steps_u) {
            break;
        }

        vec3 p = ro + t * rd;
        FieldSample s = field_eval(p);
        aux = s.aux;

        float d = s.d;
        if (isnan(d))
            d = 1e-3;

        float eps = max(hit_eps, 1e-3 * t); // grows with distance
        if (d < eps) {
            float lo = t_prev;
            float hi = t;
            for (int r = 0; r < 16; ++r) {
                float mid = 0.5 * (lo + hi);
                float dm = field_eval(ro + mid * rd).d;
                float em = max(hit_eps, 1e-3 * mid);
                if (dm < em) {
                    hi = mid;
                } else {
                    lo = mid;
                }
            }
            t = hi;
            hit = true;
            break;
        }

        t_prev = t;

        // Clamp step to avoid stalls / negative weirdness
        float step = d * step_scale;
        step = clamp(step, 1e-5, 0.5);
        t += step;

        if (t > max_dist) {
            break;
        }
    }

    if (!hit) {
        o_color = vec4(bg.r, bg.g, bg.b, 1.0);
        return;
    }

    vec3 p = ro + t * rd;
    vec3 n = estimate_normal(p, t);

    vec3 l = normalize(vec3(0.6, 0.7, 0.2));
    float ndotl = max(dot(n, l), 0.0);

    float s = float(steps) / float(max(max_steps_u, 1));
    float c = clamp(aux * 0.25, 0.0, 1.0);

    vec3 base = mix(vec3(0.2, 0.3, 0.6), vec3(0.9, 0.8, 0.2), c);

    float ao = ambient_occlusion(p, n);
    vec3 col = base * (0.10 + 0.90 * ndotl) * ao;

    // Put step count into red channel slightly (debug)
    col.r = clamp(col.r + 0.35 * s, 0.0, 1.0);

    o_color = vec4(col, 1.0);
}
