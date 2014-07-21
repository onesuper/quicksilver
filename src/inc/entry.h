#pragma once


#include <stdio.h>

struct Entry {  
  Entry *prev;
  Entry *next;

  virtual void printf() {};
};


class ThreadEntry : public Entry {

private:
  int _thread_index;

public:
  ThreadEntry(int tid) {
    _thread_index = tid;
  }

  void print(void) {
    printf("%d\t", _thread_index);
  }
};