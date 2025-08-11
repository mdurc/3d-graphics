#pragma once

typedef enum {
  INPUT_KEY_LEFT,
  INPUT_KEY_RIGHT,
  INPUT_KEY_UP,
  INPUT_KEY_DOWN,
  INPUT_KEY_ESCAPE,
  INPUT_KEY_DEBUG,
  INPUT_KEY_EDITOR_TOGGLE,
  INPUT_KEY_W,
  INPUT_KEY_A,
  INPUT_KEY_S,
  INPUT_KEY_D,
  INPUT_KEY_SPACE,

  INPUT_KEY_COUNT,
} input_key_t;

typedef enum {
  KS_UNPRESSED,
  KS_PRESSED,
  KS_HELD,
} key_state_t;

typedef struct {
  key_state_t states[INPUT_KEY_COUNT];
} input_state_t;

void input_update(void);
