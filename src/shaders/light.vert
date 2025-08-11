#version 330 core
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_tex_coords;

smooth out vec4 v_color;
out vec2 tex_coords;

uniform mat4 u_model;
uniform mat4 u_view_proj;
uniform mat3 u_normal_matrix; // inverse transpose of the model done on CPU

uniform vec3 u_dir_to_light;
uniform vec4 u_light_color; // incorporates light intensity
uniform vec4 u_object_color;

uniform vec4 u_ambient_intensity;

void main() {
  vec4 ambient = u_ambient_intensity * u_light_color;

  vec3 cam_normal = normalize(u_normal_matrix * a_normal);
  vec3 light_normal = normalize(u_dir_to_light);
  float cos_ang_incidence = max(dot(cam_normal, light_normal), 0.0);

  vec4 diffuse = u_light_color * cos_ang_incidence;

  v_color = (ambient + diffuse) * u_object_color;
  tex_coords = a_tex_coords;
  gl_Position = u_view_proj * u_model * vec4(a_pos, 1.0);
}
