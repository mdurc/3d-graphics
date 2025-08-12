#version 330 core
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec2 a_tex_coords;

out vec2 v_tex_coords;

uniform mat4 u_model; // world transform
uniform mat4 u_view_proj; // viewport transform

void main() {
  gl_Position = u_view_proj * u_model * vec4(a_pos, 1.0);
  v_tex_coords = a_tex_coords;
}
