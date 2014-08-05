#pragma once


#include <cstdio>

#ifdef DEBUG
  #undef DEBUG
  #define DEBUG(...) \
            fflush(stdout); \
            fprintf(stdout, "[%30s:%-4d] ", __FILE__, __LINE__); \
            fprintf(stdout, __VA_ARGS__); \
            fprintf(stdout, "\n");
#else
  #define DEBUG(...)          
#endif




