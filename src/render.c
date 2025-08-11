#include "render.h"

#include <glad/glad.h>
#include <stddef.h>

#include "c-lib/math.h"
#include "c-lib/misc.h"
#include "file_io.h"

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

static mat4x4 view_proj;

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
      // back face
      {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0, 0}},
      {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0, 0}},
      {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0, 0}},
      {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0, 0}},
      // front face
      {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0, 0}},
      {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0, 0}},
      {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0, 0}},
      {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0, 0}},
      // left face
      {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {0, 0}},
      {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0, 0}},
      {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0, 0}},
      {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {0, 0}},
      // right face
      {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0, 0}},
      {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0, 0}},
      {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0, 0}},
      {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0, 0}},
      // bottom face
      {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0, 0}},
      {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0, 0}},
      {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0, 0}},
      {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0, 0}},
      // top face
      {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0, 0}},
      {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0, 0}},
      {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0, 0}},
      {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0, 0}},
  };

  // note the winding order - counter clockwise
  u32 indices[] = {
      0,  1,  2,  2,  3,  0,  // back
      4,  5,  6,  6,  7,  4,  // front
      8,  9,  10, 10, 11, 8,  // left
      12, 13, 14, 14, 15, 12, // right
      16, 17, 18, 18, 19, 16, // bottom
      20, 21, 22, 22, 23, 20  // top
  };
  return create_mesh(vertices, sizeof(vertices), indices, sizeof(indices));
}

static mesh_t create_ramp_mesh(void) {
  vertex_t vertices[] = {
      // bottom face (y = -0.5)
      {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0, 0}},  // 0
      {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0, 0}},   // 1
      {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0, 0}},  // 2
      {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0, 0}}, // 3

      // back face (x = -0.5)
      {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0, 0}}, // 4
      {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0, 0}},  // 5
      {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {0, 0}},  // 6
      {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {0, 0}},   // 7

      // ramp face (angled, +X side) - normal is (0.707, 0.707, 0)
      {{-0.5f, 0.5f, -0.5f}, {0.707f, 0.707f, 0.0f}, {0, 0}}, // 8
      {{0.5f, -0.5f, -0.5f}, {0.707f, 0.707f, 0.0f}, {0, 0}}, // 9
      {{0.5f, -0.5f, 0.5f}, {0.707f, 0.707f, 0.0f}, {0, 0}},  // 10
      {{-0.5f, 0.5f, 0.5f}, {0.707f, 0.707f, 0.0f}, {0, 0}},  // 11

      // side face (z = 0.5)
      {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0, 0}}, // 12
      {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0, 0}},  // 13
      {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0, 0}},  // 14

      // side face (z = -0.5)
      {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0, 0}}, // 15
      {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0, 0}},  // 16
      {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0, 0}},  // 17
  };

  u32 indices[] = {
      0,  1,  2,  0, 2,  3,  // bottom
      4,  5,  6,  5, 7,  6,  // back
      8,  9,  10, 8, 10, 11, // ramp
      12, 13, 14,            // side 1 (0.5)
      15, 16, 17,            // side 2 (-0.5)
  };

  return create_mesh(vertices, sizeof(vertices), indices, sizeof(indices));
}

static material_t create_material(u32 shader_prog, vec4 color) {
  return (material_t){
      .shader_program = shader_prog,
      .color = {color[0], color[1], color[2], color[3]},
  };
}

static void update_models(GLFWwindow* window, f32 angle) {
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  f32 aspect_ratio = (f32)width / (f32)height;

  mat4x4 rotation;
  for (u32 i = 0; i < object_count; ++i) {
    f32 t = angle * (i == 0 ? -1.0f : 1.0f);
    mat4x4_identity(rotation);
    mat4x4_rotate_y(rotation, rotation, t);
    mat4x4_rotate_x(rotation, rotation, t / 2.0f);
    mat4x4_rotate_z(rotation, rotation, t / 3.0f);
    mat4x4_identity(objects[i].model);
    mat4x4_translate(objects[i].model, i == 0 ? -1.0f : 1.0f, 0.0f, 0.0f);
    mat4x4_mul(objects[i].model, objects[i].model, rotation);
  }

  mat4x4 view;
  vec3 camera_front;
  get_camera_front(camera_front);

  vec3 camera_target;
  vec3_add(camera_target, camera.position, camera_front);
  mat4x4_look_at(view, camera.position, camera_target,
                 (vec3){0.0f, 1.0f, 0.0f});

  mat4x4 proj;
  mat4x4_perspective(proj, RAD(45.0f), aspect_ratio, 0.1f, 100.0f);
  mat4x4_mul(view_proj, proj, view);
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

  // u32 default_prog = create_shader_program("src/shaders/default.vert",
  //                                          "src/shaders/default.frag");
  u32 light_prog =
      create_shader_program("src/shaders/light.vert", "src/shaders/light.frag");

  meshes[0] = create_cube_mesh();
  meshes[1] = create_ramp_mesh();
  materials[0] = create_material(light_prog, RED);
  materials[1] = create_material(light_prog, BLUE);

  object_count = 2;
  objects[0] = (render_object_t){.mesh = &meshes[0], .material = &materials[0]};
  objects[1] = (render_object_t){.mesh = &meshes[1], .material = &materials[1]};

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

static void render_object(GLFWwindow* window, render_object_t* object) {
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

  mat4x4 normal_matrix;
  mat4x4_invert(normal_matrix, object->model);
  mat4x4_transpose(normal_matrix, normal_matrix);

  // transpose is true, because we are tracking in row major format formats
  glUniformMatrix4fv(glGetUniformLocation(prog, "u_model"), 1, GL_TRUE,
                     &object->model[0][0]);
  glUniformMatrix4fv(glGetUniformLocation(prog, "u_view_proj"), 1, GL_TRUE,
                     (const GLfloat*)view_proj);
  glUniformMatrix3fv(glGetUniformLocation(prog, "u_normal_matrix"), 1, GL_TRUE,
                     &normal_matrix[0][0]);
  glUniform3fv(glGetUniformLocation(prog, "u_dir_to_light"), 1,
               (vec3){1.0f, 1.0f, 0.5f});
  glUniform4fv(glGetUniformLocation(prog, "u_light_color"), 1,
               (vec4){0.8f, 0.8f, 0.8f, 1.0f}); // white light at 80% intensity
  glUniform4fv(glGetUniformLocation(prog, "u_object_color"), 1,
               object->material->color);
  glUniform4fv(glGetUniformLocation(prog, "u_ambient_intensity"), 1,
               (vec4){0.2f, 0.2f, 0.2f, 1.0f});

  glBindVertexArray(object->mesh->vao);
  glDrawElements(GL_TRIANGLES, object->mesh->index_count, GL_UNSIGNED_INT,
                 NULL);
  glBindVertexArray(0);
}

void render_cube(GLFWwindow* window) { render_object(window, &objects[0]); }
void render_ramp(GLFWwindow* window) { render_object(window, &objects[1]); }
