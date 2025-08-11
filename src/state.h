#pragma once

#include "config/config.h"
#include "config/input.h"
#include "time_state.h"

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

typedef struct {
  GLFWwindow* window;
  time_state_t time;

  input_state_t input;
  config_t config;
} state_t;

extern state_t state;
