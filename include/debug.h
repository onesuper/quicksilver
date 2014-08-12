#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <cstdio>

#ifdef DEBUG
  #undef DEBUG
  #define DEBUG(...) \
            fflush(stdout); \
            fprintf(stdout, "# "); \
            fprintf(stdout, __VA_ARGS__); \
            fprintf(stdout, "\n");
#else
  #define DEBUG(...)          
#endif



#endif
