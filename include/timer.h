#pragma once

#include <limits.h>

// cycle per millis
// e.g. 1GHz = 1000 000 000 cycle/s 
#define CPU_FREQ 1

typedef struct {
  unsigned int high;
  unsigned int low;
} timeinfo_t;



class Timer {

private:
  timeinfo_t _start;
  timeinfo_t _end;

  void __get_time(timeinfo_t &ti) {
    unsigned int l, h;
    asm volatile ("rdtsc" : "=a"(l), "=d"(h));
    ti.low = l;
    ti.high = h;
  }

public:

  void start(void) {
    __get_time(_start);
    return;
  }
  
  void stop(void) {
    __get_time(_end);
  }

  double elapsedCycles(void) {
    double cycles = 0.0;
    
    cycles = (double) (_end.low - _start.low) + (double) (UINT_MAX) 
      * (double) (_end.high - _start.high);
   
    if (_end.low < _start.low)
      cycles -= (double) UINT_MAX;
      
    return cycles;
  }
  
  
  unsigned long elapsedMillis(void) {
    unsigned long millis;
    double cycles = 0.0;
    
    cycles = (double) (_end.low - _start.low) + (double) (UINT_MAX) 
      * (double) (_end.high - _start.high);
   
    if (_end.low < _start.low)
      cycles -= (double) UINT_MAX;
    
    millis = (unsigned long) (cycles / CPU_FREQ);
    
    return millis;
  }


};
