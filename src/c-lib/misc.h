#ifndef _LIB_MISC_H
#define _LIB_MISC_H

#include <stdio.h>
#include <stdlib.h> // for exit(1);

#include "log.h"
#include "macros.h"

#define _ASSERTX(_e, _expr, _fmt, ...)      \
  do {                                      \
    if (!(_e)) {                            \
      ERROR("assertion failed: (%s): " _fmt, _expr, ##__VA_ARGS__); \
      exit(1);                              \
    }                                       \
  } while (0)
#define _ASSERT1(_e) _ASSERTX(_e, #_e, "%s", "assertion failed")
#define _ASSERT2(_e, ...) _ASSERTX(_e, #_e, __VA_ARGS__)
#define _ASSERT3(_e, ...) _ASSERTX(_e, #_e, __VA_ARGS__)
#define _ASSERT4(_e, ...) _ASSERTX(_e, #_e, __VA_ARGS__)
#define _ASSERT5(_e, ...) _ASSERTX(_e, #_e, __VA_ARGS__)
#define _ASSERT6(_e, ...) _ASSERTX(_e, #_e, __VA_ARGS__)
#define _ASSERT7(_e, ...) _ASSERTX(_e, #_e, __VA_ARGS__)
#define _ASSERT8(_e, ...) _ASSERTX(_e, #_e, __VA_ARGS__)
#define ASSERT(...) DISPATCH_N(_ASSERT, __VA_ARGS__)

#define ERROR_EXIT(...) fprintf(stderr, __VA_ARGS__); exit(1)
#define ERROR_RETURN(_R, ...) fprintf(stderr, __VA_ARGS__); return _R

#define ARRLEN(_arr) ((sizeof((_arr))) / ((sizeof((_arr)[0]))))

#define R_MASK 0x000000FF
#define G_MASK 0x0000FF00
#define B_MASK 0x00FF0000
#define A_MASK 0xFF000000

#define WHITE (vec4){1, 1, 1, 1}
#define BLACK (vec4){0, 0, 0, 1}
#define RED (vec4){1, 0, 0, 1}
#define GREEN (vec4){0, 1, 0, 1}
#define BLUE (vec4){0, 0, 1, 1}
#define YELLOW (vec4){1, 1, 0, 1}
#define CYAN (vec4){0, 1, 1, 1}
#define MAGENTA (vec4){1, 0, 1, 1}
#define ORANGE (vec4){1, 0.5, 0, 1}
#define PURPLE (vec4){0.5, 0, 1, 1}
#define TURQUOISE (vec4){0, 1, 0.5, 1}

#endif
