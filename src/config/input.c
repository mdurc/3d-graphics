#include "input.h"

#include "../state.h"

static void update_key_state(bool is_down, key_state_t* key_state) {
  if (is_down) {
    *key_state = (*key_state == KS_PRESSED || *key_state == KS_HELD)
                     ? KS_HELD
                     : KS_PRESSED;
  } else {
    *key_state = KS_UNPRESSED;
  }
}

void input_update(void) {
  glfwPollEvents();

  for (u32 i = 0; i < INPUT_KEY_COUNT; ++i) {
    u32 key_code = state.config.keybinds[i];
    bool is_down = glfwGetKey(state.window, key_code) == GLFW_PRESS;
    update_key_state(is_down, &state.input.states[i]);
  }
}
