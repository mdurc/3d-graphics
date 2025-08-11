#pragma once

#include "c-lib/math.h"
#include "c-lib/types.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

typedef struct {
  vec3 position;
  vec3 normal;
  vec2 tex_coords;
} vertex_t;

typedef struct {
  u32 vao;
  u32 vbo;
  u32 ebo;
  u32 index_count;
} mesh_t; // raw geometry on the GPU

typedef struct {
  u32 shader_program;
  vec4 color;
  // texture handles
} material_t; // appearance of an object

typedef struct {
  mesh_t* mesh;
  material_t* material;
  mat4x4 model;
  mat4x4 view_proj;
} render_object_t; // single drawable object

typedef struct {
  vec3 position;
  f32 yaw;   // (x) in degrees
  f32 pitch; // (y) in degrees
  f64 last_x;
  f64 last_y;
  bool first_mouse;
} camera_t;

GLFWwindow* render_init(u32 width, u32 height);
void render_destroy(GLFWwindow* window);

camera_t* get_camera(void);
void get_camera_front(vec3 result);

void render_begin(void);
void render_end(GLFWwindow* window);

void render_cube(GLFWwindow* window);
