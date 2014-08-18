#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <cstdio>

#ifdef DEBUG
  #undef DEBUG
  #define DEBUG(...) \
            fflush(stdout); \
            fprintf(stdout, __VA_ARGS__); \

#else
  #define DEBUG(...)          
#endif



#endif
