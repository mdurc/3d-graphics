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

static mesh_t cube_mesh;
static material_t cube_material;

static render_object_t cube;

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

  int framebuffer_width, framebuffer_height;
  glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
  glViewport(0, 0, framebuffer_width, framebuffer_height);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

  glEnable(GL_DEPTH_TEST);

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
    printf("Shader Compilation Error: %s\n", log);
    return 1;
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
    printf("Shader Program Linking Error: %s\n", log);
    return 1;
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

static material_t create_material(u32 shader_prog, vec4 color) {
  return (material_t){
      .shader_program = shader_prog,
      .color = {color[0], color[1], color[2], color[3]},
  };
}

static void cube_transform(f32 angle) {
  mat4x4 model;
  mat4x4_identity(model);
  mat4x4_translate(model, 0.0f, 0.0f, 0.0f);

  mat4x4 rotation;
  mat4x4_identity(rotation);
  mat4x4_rotate_y(rotation, rotation, angle);
  mat4x4_rotate_x(rotation, rotation, angle / 2.0f);
  mat4x4_rotate_z(rotation, rotation, angle / 3.0f);

  mat4x4_mul(model, model, rotation);

  mat4x4 view, projection;
  mat4x4_identity(view);
  mat4x4_translate(view, 0.0f, 0.0f, -2.0f);
  mat4x4_perspective(projection, M_PI / 2.0f, 1.0f, 0.1f, 100.0f);

  mat4x4 view_proj;
  mat4x4_mul(view_proj, projection, view);

  mat4x4_mul(cube.transform, view_proj, model);
}

GLFWwindow* render_init(u32 width, u32 height) {
  GLFWwindow* window = init_window(width, height);

  vertex_t vertices[] = {
      // position, normal, tex coords
      {{-0.5f, -0.5f, -0.5f}, {0, 0, 0}, {0, 0}}, // back bot left
      {{0.5f, -0.5f, -0.5f}, {0, 0, 0}, {0, 0}},  // back bot right
      {{0.5f, 0.5f, -0.5f}, {0, 0, 0}, {0, 0}},   // back top right
      {{-0.5f, 0.5f, -0.5f}, {0, 0, 0}, {0, 0}},  // back top left
      {{-0.5f, -0.5f, 0.5f}, {0, 0, 0}, {0, 0}},  // front bot left
      {{0.5f, -0.5f, 0.5f}, {0, 0, 0}, {0, 0}},   // front bot right
      {{0.5f, 0.5f, 0.5f}, {0, 0, 0}, {0, 0}},    // front top right
      {{-0.5f, 0.5f, 0.5f}, {0, 0, 0}, {0, 0}},   // front top left
  };

  // note the winding order - counter clockwise
  u32 indices[] = {
      0, 1, 2, 2, 3, 0, // back face
      4, 5, 6, 6, 7, 4, // front face
      4, 0, 3, 3, 7, 4, // left face
      1, 5, 6, 6, 2, 1, // right face
      4, 5, 1, 1, 0, 4, // bottom face
      3, 2, 6, 6, 7, 3, // top face
  };

  u32 default_prog = create_shader_program("src/shaders/default.vert",
                                           "src/shaders/default.frag");

  cube_mesh = create_mesh(vertices, sizeof(vertices), indices, sizeof(indices));
  cube_material = create_material(default_prog, WHITE);

  cube.mesh = &cube_mesh;
  cube.material = &cube_material;

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
  destroy_mesh(&cube_mesh);
  destroy_material(&cube_material);
  glfwDestroyWindow(window); // optional
  glfwTerminate();
}

void render_begin(void) {
  glClearColor(0.2f, 0.2f, 0.2f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void render_end(GLFWwindow* window) { glfwSwapBuffers(window); }

void render_cube(void) {
  cube_transform(glfwGetTime());
  ASSERT(cube.mesh && cube.material);

  u32 prog = cube.material->shader_program;
  glUseProgram(prog);

  // transpose is true, because we are tracking in row major format formats
  glUniformMatrix4fv(glGetUniformLocation(prog, "u_transform"), 1, GL_TRUE,
                     &cube.transform[0][0]);
  glUniform4fv(glGetUniformLocation(prog, "u_color"), 1, cube.material->color);

  glBindVertexArray(cube.mesh->vao);
  glDrawElements(GL_TRIANGLES, cube.mesh->index_count, GL_UNSIGNED_INT, NULL);
}

/* lighting

   Diffuse Lighting Equation: R = D * I * cos(T)
   : R -> reflected color
   : D -> Diffuse surface absorption
   : I -> Light intensity
   : T -> (theta) angle of incidence

   When the angle of incidence is zero, then the light is being pointed directly
   at the target, thus, the reflected color is at its brightest.
   When the angle is 90 deg, no light should be reflected, and thus cos(90 deg)
   is accurately zero.

   Any values will be clamped to zero being the minimum.

   For regular triangles, each vertex in the same face has the same surface
   normal. For curved polygonal objects, this cannot be the case in order to
   create a realistic calculation. Thus, we can assign each specific vertex its
   own normal (approximated).

   However, even this is not quite sufficient. The vertex shader with the vertex
   data is only used for rasterization, not for actually generating any visual
   material, this is the job of the fragment shader...

   "Gouraud shading" is used to perform lighting computations at every vertex,
   and let the result be interpolated across the surface of the triangle.

   This is generally a fast process, and not having to do the computations at
   every fragment generated from the triangle is very good.

   Note that this is not the standard for modern game design, it is too slow.

   When you have a light source, the angle of incidence is generally different
   between each point on a given face. For example, as the source moves farther
   away, the angles will begin to straighten out to a zero degree angle of
   incidence, and when the source comes really close, the outer most vertices
   will have extreme angles and the center vertices will have a perfect zero deg
   angle.

   To create a realistic lighting system,  we must include "ambient lighting".
   Real global illumination is very difficult, so this model is often used
   instead. It assumes that there is a light of a certain intensity that
   emanates from everywhere. It comes from all directions equally, so there is
   no angle of incidence in the diffuse calculation. Thus, the formula is
   simply: ambient light intensity * diffuse surface color.
*/
