#pragma once

#include "../c-lib/types.h"
#include "input.h"

typedef struct {
  u32 keybinds[INPUT_KEY_COUNT];
} config_t;

typedef struct {
  const char* name;
  i32 glfw_key_code;
} keymap_t;

typedef struct {
  input_key_t key;
  const char* name_in_config; // "left", "editor", "debug"
  const char* default_key;    // "Left", "O", "A"
} keybind_info_t;

void config_init(void);
void config_key_bind(input_key_t key, const char* key_name);
