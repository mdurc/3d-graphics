#include "config.h"

#include <ctype.h>
#include <stdlib.h>

#include "../c-lib/misc.h"
#include "../file_io.h"
#include "../state.h"

static const keymap_t glfw_keymap[] = {
    {"Left", GLFW_KEY_LEFT}, {"Right", GLFW_KEY_RIGHT},   {"Up", GLFW_KEY_UP},
    {"Down", GLFW_KEY_DOWN}, {"Escape", GLFW_KEY_ESCAPE}, {"F", GLFW_KEY_F},
    {"O", GLFW_KEY_O},       {"Space", GLFW_KEY_SPACE},   {"W", GLFW_KEY_W},
    {"A", GLFW_KEY_A},       {"S", GLFW_KEY_S},           {"D", GLFW_KEY_D},
};
static const keybind_info_t config_info[] = {
    // NOTE: this order should match the order of the input_key_t enums
    {INPUT_KEY_LEFT, "left", "Left"},
    {INPUT_KEY_RIGHT, "right", "Right"},
    {INPUT_KEY_UP, "up", "Up"},
    {INPUT_KEY_DOWN, "down", "Down"},
    {INPUT_KEY_ESCAPE, "escape", "Escape"},
    {INPUT_KEY_DEBUG, "debug", "F"},
    {INPUT_KEY_EDITOR_TOGGLE, "editor", "O"},
    {INPUT_KEY_W, "w", "W"},
    {INPUT_KEY_A, "a", "A"},
    {INPUT_KEY_S, "s", "S"},
    {INPUT_KEY_D, "d", "D"},
    {INPUT_KEY_SPACE, "jump", "Space"},
};
static const size_t glfw_keymap_size = sizeof(glfw_keymap) / sizeof(keymap_t);
static const size_t config_size = sizeof(config_info) / sizeof(keybind_info_t);

static void config_get_value(char* dest, u32 dest_size, const char* conf_buf,
                             const char* value) {
  const char* search_start = conf_buf;
  const size_t key_len = strlen(value);

  while (1) {
    const char* key_ptr = strstr(search_start, value);

    ASSERT(key_ptr, "couldn't find the config key: %s\n", value);

    // check if the found occurrence is a valid key
    // a valid key is at the start of a line and is a whole word
    bool is_at_line_start = (key_ptr == conf_buf || *(key_ptr - 1) == '\n');
    char char_after_key = *(key_ptr + key_len);
    bool is_whole_word = (isspace(char_after_key) || char_after_key == '=');

    if (is_at_line_start && is_whole_word) {
      // parse the whole word
      char* curr = (char*)key_ptr + key_len;
      char* dest_ptr = dest;

      // move to the '=' sign and skip it
      while (*curr != '=' && *curr != '\0') ++curr;
      if (*curr == '=') ++curr;
      while (isspace(*curr)) ++curr;

      // copy the value until a newline or end of buffer
      while (*curr != '\n' && *curr != '\r' && *curr != '\0' &&
             (dest_ptr - dest) < (dest_size - 1)) {
        *dest_ptr++ = *curr++;
      }
      *dest_ptr = '\0'; // null-terminate the destination string

      return;
    }
    // else continue searching, false position (ex. 'w' in 'down')
    search_start = key_ptr + 1;
  }
}

static i32 key_from_name(const char* name) {
  for (size_t i = 0; i < glfw_keymap_size; ++i) {
    if (strcmp(name, glfw_keymap[i].name) == 0) {
      return glfw_keymap[i].glfw_key_code;
    }
  }
  return -1;
}

static void config_load_controls(const char* conf_buf) {
  char key_name_buf[64];
  for (size_t i = 0; i < config_size; ++i) {
    const keybind_info_t* info = &config_info[i];
    config_get_value(key_name_buf, sizeof(key_name_buf), conf_buf,
                     info->name_in_config);
    config_key_bind(info->key, key_name_buf);
  }
}

static i32 config_load(void) {
  file_t config_file = io_file_read("./config.ini");
  if (!config_file.is_valid) {
    return -1;
  }
  config_load_controls(config_file.data);
  free(config_file.data);
  return 0;
}

static void write_default_config(void) {
  char buffer[1024] = "[controls]\n";
  for (size_t i = 0; i < config_size; ++i) {
    const keybind_info_t* info = &config_info[i];
    char line[128];
    snprintf(line, sizeof(line), "%s = %s\n", info->name_in_config,
             info->default_key);
    strcat(buffer, line);
  }
  io_file_write(buffer, strlen(buffer), "./config.ini");
  LOG("Wrote and loaded a default config to disk at: ./config.ini");
}

void config_init(void) {
  if (config_load() == 0) {
    LOG("Configuration system initialized and loaded");
    return;
  }

  write_default_config();
  LOG("wrote and loaded a default config to disk at: ./config.ini");
  if (config_load() != 0) {
    ERROR_EXIT("default config did not properly load.");
  }
}

void config_key_bind(input_key_t key, const char* key_name) {
  i32 key_code = key_from_name(key_name);
  if (key_code == -1) {
    ERROR_EXIT("invalid key name when binding key: %s\n", key_name);
  }
  ASSERT(key_code >= 0);
  state.config.keybinds[key] = key_code;
}
