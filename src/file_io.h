#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  char* data;
  size_t len;
  bool is_valid;
} file_t;

file_t io_file_read(const char* path);
int io_file_write(void* buf, size_t size, const char* path);
