#include "render.h"

#include <glad/glad.h>
#include <stddef.h>

#include "c-lib/math.h"
#include "c-lib/misc.h"
#include "file_io.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define EN_ATTRIB(_n, _sz, _member) \
  glVertexAttribPointer(_n, _sz, GL_FLOAT, GL_FALSE, \
      sizeof(vertex_t), (void*)offsetof(vertex_t, _member)); \
  glEnableVertexAttribArray(_n);

#define RAD(_t) (_t * (M_PI / 180.0f))

#define MAX_OBJECTS 10
static mesh_t meshes[MAX_OBJECTS];
static material_t materials[MAX_OBJECTS];
static render_object_t objects[MAX_OBJECTS];
static u32 object_count = 0;
static camera_t camera;

static vec3 light_pos = (vec3){0.0f, 0.0f, 3.0f};

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
static mesh_t create_mesh(vertex_t* vertices, u32 vertices_size, u32* indices,
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

  EN_ATTRIB(0, 3, position);   // [x, y, z]
  EN_ATTRIB(1, 3, normal);     // [x, y, z]
  EN_ATTRIB(2, 2, tex_coords); // [u, v]

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
  vertex_t vertices[] = {
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
  vertex_t vertices[] = {
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
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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

static void update_models(GLFWwindow* window, f32 angle) {
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  f32 aspect_ratio = (f32)width / (f32)height;

  mat4x4 rotation;
  mat4x4_identity(rotation);
  // mat4x4_rotate_y(rotation, rotation, angle);
  mat4x4_rotate_x(rotation, rotation, angle / 2.0f);
  // mat4x4_rotate_z(rotation, rotation, angle / 3.0f);

  mat4x4_identity(objects[0].model);
  mat4x4_identity(objects[1].model);
  mat4x4_identity(objects[2].model);
  mat4x4_translate(objects[0].model, -1.0f, 0.0f, 0.0f);
  mat4x4_translate(objects[1].model, 1.0f, 0.0f, 0.0f);
  mat4x4_translate(objects[2].model, light_pos[0], light_pos[1], light_pos[2]);

  mat4x4_mul(objects[0].model, objects[0].model, rotation);
  mat4x4_mul(objects[1].model, objects[1].model, rotation);
  mat4x4_scale_aniso(objects[2].model, objects[2].model, 0.2f, 0.2f, 0.2f);

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

  stbi_set_flip_vertically_on_load(1);

  u32 tex_cube = create_texture("res/map_wall.png");
  u32 tex_ramp = create_texture("res/map_floor.png");
  u32 tex_white = create_white_texture();

  u32 default_prog = create_shader_program("src/shaders/default.vert",
                                           "src/shaders/default.frag");
  u32 light_prog =
      create_shader_program("src/shaders/light.vert", "src/shaders/light.frag");

  meshes[0] = create_cube_mesh();
  meshes[1] = create_ramp_mesh();
  materials[0] = create_material(light_prog, TURQUOISE, tex_cube);
  materials[1] = create_material(light_prog, RED, tex_white);
  materials[2] = create_material(default_prog, YELLOW, tex_white);

  object_count = 3;
  objects[0] = (render_object_t){.mesh = &meshes[0], .material = &materials[0]};
  objects[1] = (render_object_t){.mesh = &meshes[1], .material = &materials[1]};
  objects[2] = (render_object_t){.mesh = &meshes[0], .material = &materials[2]};

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

void render_end(GLFWwindow* window) { glfwSwapBuffers(window); }

static void render_object(GLFWwindow* window, render_object_t* object,
                          bool lit) {
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
  update_models(window, glfwGetTime());
  ASSERT(object->mesh && object->material);

  u32 prog = object->material->shader_program;
  glUseProgram(prog);

  glUniform1i(glGetUniformLocation(prog, "u_texture0"), 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, object->material->texture_id);

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

  glBindVertexArray(object->mesh->vao);
  glDrawElements(GL_TRIANGLES, object->mesh->index_count, GL_UNSIGNED_INT,
                 NULL);
  glBindVertexArray(0);
}

void render_cube(GLFWwindow* window) {
  render_object(window, &objects[0], true);
}
void render_ramp(GLFWwindow* window) {
  render_object(window, &objects[1], true);
}
void render_light(GLFWwindow* window) {
  render_object(window, &objects[2], false);
}
