#pragma once


#include <stdio.h>


class ThreadEntry : public Entry {

private:
  int _thread_index;

public:

  ThreadEntry():
  _thread_index(0)
  {}
   
  ThreadEntry(int tid) {
    _thread_index = tid;
  }

  int getIndex() {
    return _thread_index;
  }

  void print(void) {
    DEBUG("  %d  ", _thread_index);
  }
};
