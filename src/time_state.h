#pragma once

#include "c-lib/types.h"

typedef struct {
  f32 delta, now, last;
  f32 frame_last, frame_delay, frame_time;
  u32 frame_rate, frame_count;
} time_state_t;

void time_init(f32 frame_rate);
void time_update(void);
void time_update_late(void);
