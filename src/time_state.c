#include <math.h>
#include <time.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "c-lib/log.h"
#include "state.h"
#include "time_state.h"

void time_init(f32 frame_rate) {
  state.time.frame_rate = frame_rate;
  state.time.frame_delay = 1000.f / frame_rate;    // ms per frame
  state.time.last = (f32)(glfwGetTime() * 1000.0); // convert to ms
  LOG("Time system initialized");
}

void time_update(void) {
  state.time.now = (f32)(glfwGetTime() * 1000.0);                  // ms
  state.time.delta = (state.time.now - state.time.last) / 1000.0f; // s
  state.time.last = state.time.now;
  ++state.time.frame_count;

  if (state.time.now - state.time.frame_last >= 1000.0f) {
    state.time.frame_rate = state.time.frame_count;
    state.time.frame_count = 0;
    state.time.frame_last = state.time.now;
  }
}

void time_update_late(void) {
  state.time.frame_time = (f32)(glfwGetTime() * 1000.0) - state.time.now;

  if (state.time.frame_delay > state.time.frame_time) {
    f32 delay_ms = state.time.frame_delay - state.time.frame_time;
    struct timespec ts;
    ts.tv_sec = (time_t)(delay_ms / 1000.0f);
    ts.tv_nsec = (long)((fmod(delay_ms, 1000.0f)) * 1e6);
    nanosleep(&ts, NULL);
  }
}
