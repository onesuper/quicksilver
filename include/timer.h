#ifndef _TIMER_H_
#define _TIMER_H_


// cycle per millis
// e.g. 1GHz = 1000 000 cycle/ millis 
#define CPU_FREQ 1


typedef unsigned long long timestamp_t;

class Timer {

private:
  size_t _total;
  size_t _start_times; // How many times the timer is started
  timestamp_t _start;
  timestamp_t _end;

  inline timestamp_t now(void) {
    unsigned int lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((timestamp_t) lo) | (((timestamp_t) hi) << 32);
  }

  inline double elapsedMillis(timestamp_t from, timestamp_t to) const {
    return ((double) (to - from) / (double) CPU_FREQ);
  }

public:
  Timer(): _total(0), _start_times(0), _start(0), _end(0) {}

  void Start(void) {
    _start = now();
    _start_times++;
  }
  
  void Pause(void) {
    _end = now();
    _total += elapsedMillis(_start, _end);
  }
  
  void Reset(void) {
    _start = 0;
    _end = 0;
    _total = 0;
    _start_times = 0;
  }

  size_t Total(void) const {
    return _total;
  }
  
  size_t Times() const {
    return _start_times;
  }
};


#endif