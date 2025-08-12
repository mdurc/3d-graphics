#version 330 core
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_tex_coords;

smooth out vec4 v_color;
out vec2 v_tex_coords;

uniform mat4 u_model; // world transform
uniform mat4 u_view_proj; // viewport transform

uniform vec3 u_light_pos;
uniform vec4 u_light_color; // incorporates light intensity
uniform vec4 u_ambient_intensity;

void main() {
  gl_Position = u_view_proj * u_model * vec4(a_pos, 1.0);
  v_tex_coords = a_tex_coords;

  vec3 norm = mat3(u_model) * a_normal; // rotation component applied to normal
  float diffuse = max(dot(norm, u_light_pos), 0.0);
  v_color = (u_ambient_intensity + diffuse) * u_light_color;
}
