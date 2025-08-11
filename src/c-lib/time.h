#ifndef _LIB_TIME_H
#define _LIB_TIME_H

#include "macros.h"
#include "types.h"

#define ns_to_sec(_ns) ((_ns) / 1000000000.0)
#define sec_to_ns(_s)  ((_s) * 1000000000.0)

#ifdef CLIB_TIME_GLFW
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

M_INLINE f64 time_s(void);
M_INLINE u64 time_ns(void);

M_INLINE u64 time_ns(void) { return (u64)(glfwGetTime() * 1000000000.0); }

M_INLINE f64 time_s(void) { return glfwGetTime(); }

#elif CLIB_TIME_SDL
#include <SDL2/SDL.h>

static u64 time_start = 0, time_freq = 0;

M_INLINE f64 time_s(void);
M_INLINE u64 time_ns(void);

M_INLINE u64 time_ns(void) {
  const u64 now = SDL_GetPerformanceCounter();

  if (!time_start) {
    time_start = now;
  }

  if (!time_freq) {
    time_freq = SDL_GetPerformanceFrequency();
  }

  const u64 diff = now - time_start;
  return (diff / (f64)time_freq) * 1000000000.0;
}

M_INLINE f64 time_s(void) { return time_ns() / 1000000000.0; }

#endif
#endif
