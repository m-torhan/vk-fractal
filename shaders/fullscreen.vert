/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#version 460
layout(location = 0) out vec2 v_uv;

void main() {
  vec2 pos = vec2(
    (gl_VertexIndex == 2) ? 3.0 : -1.0,
    (gl_VertexIndex == 1) ? 3.0 : -1.0
  );
  v_uv = 0.5 * (pos + 1.0);
  gl_Position = vec4(pos, 0.0, 1.0);
}
