#ifndef _LIB_LOG_H
#define _LIB_LOG_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "macros.h"
#include "time.h"

M_INLINE void _log(const char* file, int line, const char* func,
                   const char* prefix, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  FILE* fp = !strcmp(prefix, "LOG") ? stdout : stderr;

  f64 t = time_s();
  int secs = (int)t;
  int ms = (int)((t - secs) * 1000.0);

  fprintf(fp, "[%s][%d.%03d][%s:%d][%s] ", prefix, secs, ms, file, line, func);

  const int len = vsnprintf(NULL, 0, fmt, ap);
  char buf[len + 1];
  vsnprintf(buf, len + 1, fmt, ap);
  fprintf(fp, "%s%s", buf, buf[len] == '\n' ? "" : "\n");
  va_end(ap);
}

#define LOG(_fmt, ...) _log(__FILE__, __LINE__, __FUNCTION__, "LOG", (_fmt), ##__VA_ARGS__)
#define WARN(_fmt, ...) _log( __FILE__, __LINE__, __FUNCTION__, "WRN", (_fmt), ##__VA_ARGS__)
#define ERROR(_fmt, ...) _log(__FILE__, __LINE__, __FUNCTION__, "ERR", (_fmt), ##__VA_ARGS__)

#endif
