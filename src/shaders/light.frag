#version 330 core

out vec4 frag_color;

smooth in vec4 v_color;
in vec2 v_tex_coords;

uniform sampler2D u_texture0;
uniform vec4 u_object_color;

void main() {
  frag_color = (texture(u_texture0, v_tex_coords) * u_object_color) * v_color;
}
