#pragma once


#include <stdio.h>

#ifdef DEBUG
  #define DEBUG(...) \
            fprintf(stderr, "<%s,%d>: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARG__); \
            fprintf(stderr, "\n");
#else
  #define DEBUG(...)          
#endif