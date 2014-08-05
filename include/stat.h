#pragma once

#include "timer.h"


class Stat {

private:
  Timer _timer;
  size_t _total;
  size_t _times; 

public:

  Stat(): _total(0), _times(0) {}

  void Start() {
    _timer.Start();
    _times++;
  }
  
  void Pause() {
    _timer.Stop();
    _total += _timer.ElapsedMillis();
  }
  
  size_t Total() {
    return _total;
  }
  
  size_t Times() {
    return _times;
  }
};





