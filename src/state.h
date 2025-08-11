#pragma once

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "time_state.h"

typedef struct {
  GLFWwindow* window;
  time_state_t time;
} state_t;

extern state_t state;
