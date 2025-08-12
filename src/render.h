#pragma once

#include "c-lib/math.h"
#include "c-lib/types.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

typedef struct {
  vec3 position;
  vec3 normal;
  vec2 tex_coords;
} vertex3d_t;

typedef struct {
  vec3 position;
  vec2 tex_coords;
} vertex2d_t;

typedef struct {
  u32 vao, vbo, ebo;
  u32 index_count;
} mesh_t; // raw geometry on the GPU

typedef struct {
  u32 shader_program;
  vec4 color;
  u32 texture_id;
} material_t; // appearance of an object

typedef struct {
  material_t* material;
  mesh_t* mesh;
  mat4x4 model;
} render_object_t; // single drawable object

typedef struct {
  vec3 position;
  f32 yaw;   // (x) in degrees
  f32 pitch; // (y) in degrees
  f64 last_x;
  f64 last_y;
  bool first_mouse;

  mat4x4 view_proj;
} camera_t;

typedef struct {
  material_t* material;
  mesh_t* mesh;
  f32 width, height, cell_width, cell_height;
} sprite_sheet_t; // 2d ui element sheet

GLFWwindow* render_init(u32 width, u32 height);
void render_destroy(GLFWwindow* window);

vec3* get_light_pos(void);
camera_t* get_camera(void);
void get_camera_front(vec3 result);

void render_begin(void);
void render_end(void);

void render_cube(void);
void render_ramp(void);
void render_light(void);
void render_sphere(void);
void render_quad(void);
void render_sprite_frame(f32 row, f32 column, vec2 position, vec2 size,
                         vec4 color, bool is_flipped);
