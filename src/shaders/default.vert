#version 330 core
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_tex_coords;

out vec3 normal;
out vec2 tex_coords;

uniform mat4 u_transform;

void main() {
  gl_Position = u_transform * vec4(a_pos, 1.0);
  normal = a_normal;
  tex_coords = a_tex_coords;
}
