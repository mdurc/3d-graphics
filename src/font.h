#pragma once

#include "c-lib/math.h"

void font_render_char(char ch, vec2 position, vec2 size, vec4 color);
void font_render_str(const char* str, vec2 position, vec2 size, vec4 color);
