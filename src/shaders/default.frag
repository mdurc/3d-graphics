#version 330 core

out vec4 frag_color;

in vec3 normal;
in vec2 tex_coords;

uniform vec4 u_color;

void main() {
  frag_color = u_color;
}
