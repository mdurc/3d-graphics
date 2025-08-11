#version 330 core

out vec4 frag_color;

smooth in vec4 v_color;
in vec2 tex_coords;

void main() {
  frag_color = v_color;
}
