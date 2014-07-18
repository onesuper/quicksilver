#pragma once


#include <stdio.h>

#define ERROR(...) \
          fprintf(stderr, "[%25s:%-4d] ", __FILE__, __LINE__); \
          fprintf(stderr, __VA_ARGS__); \
          fprintf(stderr, "\n");

