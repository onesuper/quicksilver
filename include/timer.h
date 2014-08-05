#pragma once


// cycle per millis
// e.g. 1GHz = 1000 000 cycle/ millis 
#define CPU_FREQ 1000000


typedef unsigned long long timestamp_t;

class Timer {

private:
  timestamp_t _start;
  timestamp_t _end;

  static inline timestamp_t _now(void) {
    unsigned int lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((timestamp_t) lo) | (((timestamp_t) hi) << 32);
  }

public:

  void Start(void) {
    _start = _now();
    return;
  }
  
  void Stop(void) {
    _end = _now();
  }
  
  double ElapsedMillis(void) {
    double millis;
    millis = (double) (_end - _start) / (double) CPU_FREQ;
    return millis;
  }


};
