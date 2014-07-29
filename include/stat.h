#pragma once

#include "timer.h"


class Stat {

private:
  Timer _timer;
  size_t _total;
  size_t _times; 

public:

  Stat():
  _total(0),
  _times(0)
  {}

  void start() {
    _timer.start();
    _times++;
  }
  
  void pause() {
    _timer.stop();
    _total += _timer.elapsedMillis();
  }
  
  size_t getTotal() {
    return _total;
  }
  
  size_t getTimes() {
    return _times;
  }
};





