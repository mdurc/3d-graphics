#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <math.h>

#include "render.h"
#include "state.h"

state_t state;

static void input_handle(float delta_time) {
  camera_t* camera = get_camera();
  vec3* light = get_light_pos();
  const float camera_speed = 10.0f * delta_time;

  if (state.input.states[INPUT_KEY_ESCAPE]) {
    glfwSetWindowShouldClose(state.window, true);
  }

  vec3 camera_front;
  get_camera_front(camera_front);

  vec3 up_dir = (vec3){0.0f, 1.0f, 0.0f};
  vec3 right;
  vec3_cross(right, camera_front, up_dir);
  vec3_normalize(right, right);

  vec3 move_vector;
  if (state.input.states[INPUT_KEY_W]) {
    vec3_scale(move_vector, camera_front, camera_speed);
    vec3_add(camera->position, camera->position, move_vector);
  }
  if (state.input.states[INPUT_KEY_S]) {
    vec3_scale(move_vector, camera_front, camera_speed);
    vec3_sub(camera->position, camera->position, move_vector);
  }
  if (state.input.states[INPUT_KEY_A]) {
    vec3_scale(move_vector, right, camera_speed);
    vec3_sub(camera->position, camera->position, move_vector);
  }
  if (state.input.states[INPUT_KEY_D]) {
    vec3_scale(move_vector, right, camera_speed);
    vec3_add(camera->position, camera->position, move_vector);
  }
  if (state.input.states[INPUT_KEY_UP]) {
    (*light)[1] += 0.1f;
  }
  if (state.input.states[INPUT_KEY_DOWN]) {
    (*light)[1] -= 0.1f;
  }
  if (state.input.states[INPUT_KEY_LEFT]) {
    (*light)[0] -= 0.1f;
  }
  if (state.input.states[INPUT_KEY_RIGHT]) {
    (*light)[0] += 0.1f;
  }
}

int main(void) {
  state.window = render_init(1650, 1000);
  config_init();

  while (!glfwWindowShouldClose(state.window)) {
    time_update();
    input_update();

    input_handle(state.time.delta);

    render_begin();
    render_cube(state.window);
    render_ramp(state.window);
    render_light(state.window);
    render_end(state.window);

    time_update_late();
  }

  render_destroy(state.window);
  return 0;
}
