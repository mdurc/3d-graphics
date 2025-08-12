#include "render.h"

#include <glad/glad.h>
#include <stddef.h>

#include "GLFW/glfw3.h"
#include "c-lib/math.h"
#include "c-lib/misc.h"
#include "file_io.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define EN_ATTRIB(_n, _sz, _member, _vertex_type) \
  glVertexAttribPointer(_n, _sz, GL_FLOAT, GL_FALSE, \
      sizeof(_vertex_type), (void*)offsetof(_vertex_type, _member)); \
  glEnableVertexAttribArray(_n);

#define RAD(_t) (_t * (M_PI / 180.0f))

#define MAX_OBJECTS 10
static mesh_t meshes[MAX_OBJECTS];
static material_t materials[MAX_OBJECTS];
static render_object_t objects[MAX_OBJECTS];
static u32 object_count = 0;
static camera_t camera;

static vec3 light_pos = (vec3){0.0f, 0.0f, 3.0f};
static sprite_sheet_t font_sheet;

static void mouse_callback(GLFWwindow* window, f64 xpos, f64 ypos) {
  (void)window;
  if (camera.first_mouse) {
    camera.last_x = xpos;
    camera.last_y = ypos;
    camera.first_mouse = false;
  }

  f64 xoffset = xpos - camera.last_x;
  f64 yoffset = camera.last_y - ypos;
  // reversed since y-coordinates go from top to bottom
  camera.last_x = xpos;
  camera.last_y = ypos;

  f32 sensitivity = 0.1f;
  xoffset *= sensitivity;
  yoffset *= sensitivity;

  camera.yaw += xoffset;
  camera.pitch += yoffset;

  if (camera.pitch > 89.0f) camera.pitch = 89.0f;
  if (camera.pitch < -89.0f) camera.pitch = -89.0f;
}

static void framebuffer_size_callback(GLFWwindow* window, int width,
                                      int height) {
  (void)window;
  glViewport(0, 0, width, height);
}

static GLFWwindow* init_window(u32 width, u32 height) {
  ASSERT(glfwInit());
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  GLFWwindow* window = glfwCreateWindow(width, height, "window", NULL, NULL);
  if (!window) {
    glfwTerminate();
    ERROR_EXIT("failed to create glfw window");
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    ERROR_EXIT("failed to initialize glad");
  }
  return window;
}

static u32 compile_shader(const char* const shader_src, GLenum shader_type) {
  int success;
  char log[512];
  GLuint shader = glCreateShader(shader_type);
  glShaderSource(shader, 1, &shader_src, NULL);
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(shader, 512, NULL, log);
    ERROR_EXIT("Shader Compilation Error: %s", log);
  }
  return shader;
}

static u32 link_shaders(u32 shader_vert, u32 shader_frag) {
  int success;
  char log[512];
  GLuint shader_prog = glCreateProgram();
  glAttachShader(shader_prog, shader_vert);
  glAttachShader(shader_prog, shader_frag);
  glLinkProgram(shader_prog);
  glGetProgramiv(shader_prog, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(shader_prog, 512, NULL, log);
    ERROR_EXIT("Shader Program Linking Error: %s", log);
  }
  glDeleteShader(shader_vert);
  glDeleteShader(shader_frag);
  return shader_prog;
}

static u32 create_shader_program(const char* const path_vert,
                                 const char* const path_fragment) {
  file_t vert = io_file_read(path_vert);
  file_t frag = io_file_read(path_fragment);

  ASSERT(vert.is_valid && frag.is_valid);

  u32 shader_vert = compile_shader(vert.data, GL_VERTEX_SHADER);
  u32 shader_frag = compile_shader(frag.data, GL_FRAGMENT_SHADER);
  u32 shader_prog = link_shaders(shader_vert, shader_frag);

  free(vert.data);
  free(frag.data);

  return shader_prog;
}

// vertices_size and indices_size are the number of bytes in the respective arrs
static mesh_t create_mesh(vertex3d_t* vertices, u32 vertices_size, u32* indices,
                          u32 indices_size) {
  // vao is needed for rendering, can't just use the vbo
  // ebo is for element index-based rendering for reusing vertex indices
  u32 vao, vbo, ebo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);

  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, vertices_size, vertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_size, indices, GL_STATIC_DRAW);

  EN_ATTRIB(0, 3, position, vertex3d_t);   // [x, y, z]
  EN_ATTRIB(1, 3, normal, vertex3d_t);     // [x, y, z]
  EN_ATTRIB(2, 2, tex_coords, vertex3d_t); // [u, v]

  // do not unbind EBO before unbinding VAO, as the VAO tracks ebo bindings
  glBindVertexArray(0);                     // unbind vao
  glBindBuffer(GL_ARRAY_BUFFER, 0);         // unbind vbo
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // unbind ebo

  return (mesh_t){
      .vao = vao,
      .vbo = vbo,
      .ebo = ebo,
      .index_count = indices_size / sizeof(u32),
  };
}

static mesh_t create_cube_mesh(void) {
  // normally we would only need 8 vertices with the ebo, but when we
  // add lighting and normals, we need to specify each one per vertex on face
  vertex3d_t vertices[] = {
      // front face
      {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
      {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
      {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
      {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
      // back face
      {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
      {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
      {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
      {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
      // left face
      {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
      {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
      {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
      {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
      // right face
      {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
      {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
      {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
      {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
      // top face
      {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
      {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
      {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
      {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
      // bottom face
      {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
      {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
      {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
      {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
  };

  // note the winding order - counter clockwise
  u32 indices[] = {
      0,  1,  3,  1,  2,  3,  // front
      4,  5,  7,  5,  6,  7,  // back
      8,  9,  11, 9,  10, 11, // left
      12, 13, 15, 13, 14, 15, // right
      16, 17, 19, 17, 18, 19, // top
      20, 21, 23, 21, 22, 23, // bottom
  };
  return create_mesh(vertices, sizeof(vertices), indices, sizeof(indices));
}

static mesh_t create_ramp_mesh(void) {
  vertex3d_t vertices[] = {
      // front face (ramp)
      {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.707f, 0.707f}, {0.0f, 0.0f}},
      {{0.5f, -0.5f, 0.5f}, {0.0f, 0.707f, 0.707f}, {1.0f, 0.0f}},
      {{0.5f, 0.5f, -0.5f}, {0.0f, 0.707f, 0.707f}, {1.0f, 1.0f}},
      {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.707f, 0.707f}, {0.0f, 1.0f}},
      // back face
      {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
      {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
      {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
      {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
      // left side face
      {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
      {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
      {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
      // right side face
      {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
      {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
      {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
      // bottom face
      {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
      {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
      {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
      {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
  };

  u32 indices[] = {
      0,  1,  3,  1,  2,  3, // front (ramp)
      4,  5,  7,  5,  6,  7, // back
      8,  9,  10,            // left
      11, 12, 13,            // right
      14, 15, 17, 15, 16, 17 // bottom
  };

  return create_mesh(vertices, sizeof(vertices), indices, sizeof(indices));
}

static mesh_t create_sphere_mesh(u32 x_segments, u32 y_segments) {
  u32 vertices_size = (x_segments + 1) * (y_segments + 1);
  vertex3d_t* vertices =
      (vertex3d_t*)malloc(vertices_size * sizeof(vertex3d_t));
  ASSERT(vertices);
  for (u32 y = 0; y <= y_segments; ++y) {
    for (u32 x = 0; x <= x_segments; ++x) {
      f32 x_segment = (f32)x / (f32)x_segments; // normalize theta
      f32 y_segment = (f32)y / (f32)y_segments; // normalize phi
      // theta wraps around 2pi rad and phi only goes north to south: pi
      // convert spherical to cartesian coordinates
      f32 x_pos = cos(x_segment * 2.0f * M_PI) * sin(y_segment * M_PI);
      f32 y_pos = cos(y_segment * M_PI);
      f32 z_pos = sin(x_segment * 2.0f * M_PI) * sin(y_segment * M_PI);
      u32 index = y * (x_segments + 1) + x;
      vertices[index].position[0] = x_pos;
      vertices[index].position[1] = y_pos;
      vertices[index].position[2] = z_pos;
      // normals are the same as the positions in spheres, assuming const radius
      vertices[index].normal[0] = x_pos;
      vertices[index].normal[1] = y_pos;
      vertices[index].normal[2] = z_pos;
      vertices[index].tex_coords[0] = x_segment;
      vertices[index].tex_coords[1] = y_segment;
    }
  }
  u32 indices_size = x_segments * y_segments * 6;
  u32* indices = (u32*)malloc(indices_size * sizeof(u32));
  ASSERT(indices);
  u32 index = 0;
  for (u32 y = 0; y < y_segments; ++y) {
    for (u32 x = 0; x < x_segments; ++x) {
      indices[index++] = (y + 1) * (x_segments + 1) + x;
      indices[index++] = y * (x_segments + 1) + x;
      indices[index++] = y * (x_segments + 1) + x + 1;
      indices[index++] = (y + 1) * (x_segments + 1) + x;
      indices[index++] = y * (x_segments + 1) + x + 1;
      indices[index++] = (y + 1) * (x_segments + 1) + x + 1;
    }
  }
  mesh_t sphere_mesh = create_mesh(vertices, vertices_size * sizeof(vertex3d_t),
                                   indices, indices_size * sizeof(u32));
  free(vertices);
  free(indices);
  return sphere_mesh;
}

static mesh_t create_quad_mesh(void) {
  vertex2d_t vertices[] = {
      {{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}},   // top right
      {{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},  // bottom right
      {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}}, // bottom left
      {{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}},  // top left
  };

  u32 indices[] = {0, 1, 3, 1, 2, 3};

  // Since we are using a new 2D shader, we only need to enable
  // the vertex attributes that the 2D shader is using (pos and tex_coords).
  u32 vao, vbo, ebo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);

  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
               GL_STATIC_DRAW);

  EN_ATTRIB(0, 3, position, vertex2d_t);
  EN_ATTRIB(1, 2, tex_coords, vertex2d_t);

  glBindVertexArray(0);                     // unbind vao
  glBindBuffer(GL_ARRAY_BUFFER, 0);         // unbind vbo
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // unbind ebo (after vao)

  return (mesh_t){
      .vao = vao,
      .vbo = vbo,
      .ebo = ebo,
      .index_count = sizeof(indices) / sizeof(u32),
  };
}

static u32 create_white_texture(void) {
  // this will be the blank texture so that whatever color we wish to draw to
  // the object, it will be that color only
  u32 texture_id;
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);

  u8 solid_white[4] = {255, 255, 255, 255};
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               solid_white);
  glBindTexture(GL_TEXTURE_2D, 0);
  return texture_id;
}

static u32 create_texture(const char* path) {
  u32 texture_id;
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  int width, height, channel_count;
  u8* image_data = stbi_load(path, &width, &height, &channel_count, 0);
  ASSERT(image_data, "failed to load image from stb image: %s", path);
  GLenum format = channel_count == 3 ? GL_RGB : GL_RGBA;
  glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
               GL_UNSIGNED_BYTE, image_data);
  glGenerateMipmap(GL_TEXTURE_2D);

  glBindTexture(GL_TEXTURE_2D, 0);
  stbi_image_free(image_data);
  return texture_id;
}

static material_t create_material(u32 shader_prog, vec4 color, u32 texture_id) {
  return (material_t){
      .shader_program = shader_prog,
      .color = {color[0], color[1], color[2], color[3]},
      .texture_id = texture_id,
  };
}

static void update_models(f32 angle) {
  int width, height;
  glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
  f32 aspect_ratio = (f32)width / (f32)height;

  mat4x4 rotation;
  mat4x4_identity(rotation);
  // mat4x4_rotate_y(rotation, rotation, angle);
  mat4x4_rotate_x(rotation, rotation, angle / 2.0f);
  // mat4x4_rotate_z(rotation, rotation, angle / 3.0f);

  for (u32 i = 0; i < object_count; ++i) mat4x4_identity(objects[i].model);

  mat4x4_translate(objects[0].model, -1.0f, 0.0f, 0.0f);
  mat4x4_translate(objects[1].model, 1.0f, 0.0f, 0.0f);
  mat4x4_translate(objects[2].model, light_pos[0], light_pos[1], light_pos[2]);
  mat4x4_translate(objects[3].model, 0.0f, 2.0f, 0.0f);

  mat4x4_mul(objects[0].model, objects[0].model, rotation);
  mat4x4_mul(objects[1].model, objects[1].model, rotation);
  mat4x4_scale_aniso(objects[2].model, objects[2].model, 0.2f, 0.2f, 0.2f);
  mat4x4_scale_aniso(objects[3].model, objects[3].model, 0.8f, 0.8f, 0.8f);

  mat4x4 view;
  vec3 camera_front;
  get_camera_front(camera_front);

  vec3 camera_target;
  vec3_add(camera_target, camera.position, camera_front);
  mat4x4_look_at(view, camera.position, camera_target,
                 (vec3){0.0f, 1.0f, 0.0f});

  mat4x4 proj;
  mat4x4_perspective(proj, RAD(45.0f), aspect_ratio, 0.1f, 100.0f);
  mat4x4_mul(camera.view_proj, proj, view);
}

GLFWwindow* render_init(u32 width, u32 height) {
  GLFWwindow* window = init_window(width, height);

  camera = (camera_t){
      .position = {0.0f, 0.5f, 5.0f},
      .yaw = -90.0f, // look down the -z axis
      .pitch = 0.0f,
      .first_mouse = true,
  };

  int framebuffer_width, framebuffer_height;
  glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

  glViewport(0, 0, framebuffer_width, framebuffer_height);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  stbi_set_flip_vertically_on_load(1);

  u32 tex_cube = create_texture("res/map_wall.png");
  u32 tex_ramp = create_texture("res/map_floor.png");
  u32 tex_font = create_texture("res/font.png");
  u32 tex_white = create_white_texture();

  u32 default_prog = create_shader_program("src/shaders/default.vert",
                                           "src/shaders/default.frag");
  u32 light_prog =
      create_shader_program("src/shaders/light.vert", "src/shaders/light.frag");

  meshes[0] = create_cube_mesh();
  meshes[1] = create_ramp_mesh();
  meshes[2] = create_sphere_mesh(64, 64);
  meshes[3] = create_quad_mesh();
  materials[0] = create_material(light_prog, TURQUOISE, tex_cube);
  materials[1] = create_material(light_prog, WHITE, tex_ramp);
  materials[2] = create_material(light_prog, RED, tex_white);
  materials[3] = create_material(default_prog, YELLOW, tex_white);
  materials[4] = create_material(default_prog, WHITE, tex_white);
  materials[5] = create_material(default_prog, WHITE, tex_font);

  object_count = 5;
  objects[0] = (render_object_t){.mesh = &meshes[0], .material = &materials[0]};
  objects[1] = (render_object_t){.mesh = &meshes[1], .material = &materials[2]};
  objects[2] = (render_object_t){.mesh = &meshes[0], .material = &materials[3]};
  objects[3] = (render_object_t){.mesh = &meshes[2], .material = &materials[2]};
  objects[4] = (render_object_t){.mesh = &meshes[3], .material = &materials[4]};

  font_sheet.width = 128;
  font_sheet.height = 128;
  font_sheet.cell_width = 8;
  font_sheet.cell_height = 8;
  font_sheet.material = &materials[5];
  font_sheet.mesh = &meshes[3];

  LOG("Render window and geometry/meshes initialized");

  return window;
}

static void destroy_mesh(mesh_t* mesh) {
  glDeleteVertexArrays(1, &mesh->vao);
  glDeleteBuffers(1, &mesh->vbo);
  glDeleteBuffers(1, &mesh->ebo);
}
static void destroy_material(material_t* material) {
  glDeleteProgram(material->shader_program);
}
void render_destroy(GLFWwindow* window) {
  for (u32 i = 0; i < object_count; ++i) {
    destroy_mesh(&meshes[i]);
  }
  destroy_material(&materials[0]);
  glfwDestroyWindow(window); // optional
  glfwTerminate();
}

vec3* get_light_pos(void) { return &light_pos; }
camera_t* get_camera(void) { return &camera; };
void get_camera_front(vec3 result) {
  result[0] = cos(RAD(camera.yaw)) * cos(RAD(camera.pitch)); // x
  result[1] = sin(RAD(camera.pitch));                        // y
  result[2] = sin(RAD(camera.yaw)) * cos(RAD(camera.pitch)); // z
  vec3_normalize(result, result);
}

void render_begin(void) {
  glClearColor(0.2f, 0.2f, 0.2f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void render_end(void) { glfwSwapBuffers(glfwGetCurrentContext()); }

static void render_object(render_object_t* object, bool lit) {
  /* lighting

     Diffuse Lighting Equation: R = D * I * cos(T)
     : R -> reflected color
     : D -> Diffuse surface absorption
     : I -> Light intensity
     : T -> (theta) angle of incidence

     When the angle of incidence is zero, then the light is being pointed
     directly at the target, thus, the reflected color is at its brightest. When
     the angle is 90 deg, no light should be reflected, and thus cos(90 deg) is
     accurately zero.

     Any values will be clamped to zero being the minimum.

     For regular triangles, each vertex in the same face has the same surface
     normal. For curved polygonal objects, this cannot be the case in order to
     create a realistic calculation. Thus, we can assign each specific vertex
     its own normal (approximated).

     However, even this is not quite sufficient. The vertex shader with the
     vertex data is only used for rasterization, not for actually generating any
     visual material, this is the job of the fragment shader...

     "Gouraud shading" is used to perform lighting computations at every vertex,
     and let the result be interpolated across the surface of the triangle.

     This is generally a fast process, and not having to do the computations at
     every fragment generated from the triangle is very good.

     Note that this is not the standard for modern game design, it is too slow.

     When you have a light source, the angle of incidence is generally different
     between each point on a given face. For example, as the source moves
     farther away, the angles will begin to straighten out to a zero degree
     angle of incidence, and when the source comes really close, the outer most
     vertices will have extreme angles and the center vertices will have a
     perfect zero deg angle.

     To create a realistic lighting system,  we must include "ambient lighting".
     Real global illumination is very difficult, so this model is often used
     instead. It assumes that there is a light of a certain intensity that
     emanates from everywhere. It comes from all directions equally, so there is
     no angle of incidence in the diffuse calculation. Thus, the formula is
     simply: ambient light intensity * diffuse surface color.
  */
  update_models(glfwGetTime());
  ASSERT(object->mesh && object->material);

  u32 prog = object->material->shader_program;
  glUseProgram(prog);

  // transpose is true, because we are tracking in row major format formats
  glUniformMatrix4fv(glGetUniformLocation(prog, "u_model"), 1, GL_TRUE,
                     &object->model[0][0]);
  glUniformMatrix4fv(glGetUniformLocation(prog, "u_view_proj"), 1, GL_TRUE,
                     (const GLfloat*)camera.view_proj);
  glUniform4fv(glGetUniformLocation(prog, "u_object_color"), 1,
               object->material->color);
  if (lit) {
    vec3 light_pos_norm;
    vec3_mov(light_pos_norm, light_pos);
    vec3_normalize(light_pos_norm, light_pos_norm);
    glUniform3fv(glGetUniformLocation(prog, "u_light_pos"), 1, light_pos_norm);
    glUniform4fv(glGetUniformLocation(prog, "u_light_color"), 1,
                 (vec4){0.8f, 0.8f, 0.8f, 1.0f});
    glUniform4fv(glGetUniformLocation(prog, "u_ambient_intensity"), 1,
                 (vec4){0.2f, 0.2f, 0.2f, 1.0f});
  }

  glUniform1i(glGetUniformLocation(prog, "u_texture0"), 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, object->material->texture_id);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glBindVertexArray(object->mesh->vao);
  glDrawElements(GL_TRIANGLES, object->mesh->index_count, GL_UNSIGNED_INT,
                 NULL);
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

static void render_quad_impl(render_object_t* object) {
  ASSERT(object->mesh && object->material);

  int width, height;
  glfwGetWindowSize(glfwGetCurrentContext(), &width, &height);
  mat4x4_identity(object->model);
  mat4x4_translate(object->model, width * 0.5f, height * 0.5f, 0.0f);
  mat4x4_scale_aniso(object->model, object->model, 5, 5, 1.0f);
  mat4x4 ortho;
  mat4x4_ortho(ortho, 0.0f, (f32)width, 0.0f, (f32)height, -1.0f, 1.0f);

  glDisable(GL_DEPTH_TEST);
  u32 prog = object->material->shader_program;
  glUseProgram(prog);

  // transpose is true, because we are tracking in row major format formats
  glUniformMatrix4fv(glGetUniformLocation(prog, "u_model"), 1, GL_TRUE,
                     &object->model[0][0]);
  glUniformMatrix4fv(glGetUniformLocation(prog, "u_view_proj"), 1, GL_TRUE,
                     (const GLfloat*)ortho);
  glUniform4fv(glGetUniformLocation(prog, "u_object_color"), 1,
               object->material->color);

  glUniform1i(glGetUniformLocation(prog, "u_texture0"), 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, object->material->texture_id);

  // for 2d pixels
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glBindVertexArray(object->mesh->vao);
  glDrawElements(GL_TRIANGLES, object->mesh->index_count, GL_UNSIGNED_INT,
                 NULL);
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glEnable(GL_DEPTH_TEST);
}

static void calculate_sprite_tex_coords(vec4 result, f32 row, f32 column,
                                        f32 texture_width, f32 texture_height,
                                        f32 cell_width, f32 cell_height) {
  f32 w = 1.0f / (texture_width / cell_width);
  f32 h = 1.0f / (texture_height / cell_height);
  f32 x = column * w;
  // flip the row so that 0,0 index in the texture coordinates is the top left
  f32 y = ((texture_height / cell_height - 1) - row) * h;
  result[0] = x;
  result[1] = y;
  result[2] = x + w;
  result[3] = y + h;
}

void render_sprite_frame(f32 row, f32 column, vec2 position, vec2 size,
                         vec4 color, bool is_flipped) {
  ASSERT(font_sheet.material && font_sheet.mesh);

  int width, height;
  glfwGetWindowSize(glfwGetCurrentContext(), &width, &height);
  mat4x4 ortho;
  mat4x4_ortho(ortho, 0.0f, (f32)width, 0.0f, (f32)height, -1.0f, 1.0f);

  u32 prog = font_sheet.material->shader_program;
  glUseProgram(prog);

  mat4x4 model;
  mat4x4_identity(model);
  mat4x4_translate(model, position[0], position[1], 0.0f);
  mat4x4_scale_aniso(model, model, size[0], size[1], 1.0f);

  glUniformMatrix4fv(glGetUniformLocation(prog, "u_model"), 1, GL_TRUE,
                     &model[0][0]);
  glUniformMatrix4fv(glGetUniformLocation(prog, "u_view_proj"), 1, GL_TRUE,
                     (const GLfloat*)ortho);
  glUniform4fv(glGetUniformLocation(prog, "u_object_color"), 1, color);

  vec4 tex_coords;
  calculate_sprite_tex_coords(tex_coords, row, column, font_sheet.width,
                              font_sheet.height, font_sheet.cell_width,
                              font_sheet.cell_height);
  if (is_flipped) {
    f32 tmp = tex_coords[0];
    tex_coords[0] = tex_coords[2];
    tex_coords[2] = tmp;
  }

  f32 u_min = tex_coords[0];
  f32 v_min = tex_coords[1];
  f32 u_max = tex_coords[2];
  f32 v_max = tex_coords[3];

  vertex2d_t vertices[] = {
      {{0.5f, 0.5f, 0.0f}, {u_max, v_max}},   // top right
      {{0.5f, -0.5f, 0.0f}, {u_max, v_min}},  // bottom right
      {{-0.5f, -0.5f, 0.0f}, {u_min, v_min}}, // bottom left
      {{-0.5f, 0.5f, 0.0f}, {u_min, v_max}},  // top left
  };

  glBindVertexArray(font_sheet.mesh->vao);
  glBindBuffer(GL_ARRAY_BUFFER, font_sheet.mesh->vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

  glUniform1i(glGetUniformLocation(prog, "u_texture0"), 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, font_sheet.material->texture_id);

  // for 2d pixels
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glDisable(GL_DEPTH_TEST);
  glDrawElements(GL_TRIANGLES, font_sheet.mesh->index_count, GL_UNSIGNED_INT,
                 NULL);

  glEnable(GL_DEPTH_TEST);
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void render_cube(void) { render_object(&objects[0], true); }
void render_ramp(void) { render_object(&objects[1], true); }
void render_light(void) { render_object(&objects[2], false); }
void render_sphere(void) { render_object(&objects[3], true); }
void render_quad(void) { render_quad_impl(&objects[4]); }
