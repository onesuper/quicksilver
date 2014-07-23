#pragma once

#include <pthread.h>
#include "pthread.h"

#include "debug.h"


// Thread local storage: use to let thread know who he is
__thread size_t thread_index_local;
  

/**
 * This class is used to spawn threads
 */
class Thread {

private:

  ThreadFunction *_thread_func;
  void *_thread_arg;
  size_t _thread_index;

public:

  Thread():
    _thread_func(NULL),
    _thread_arg(NULL),
    _thread_index(0)
  {}
  
  static int getIndex(void) {
    return thread_index_local;
  }

    // this is a fake entry point of thread
  static void *fakeEntry(void *pthis) {
  
    Thread *obj = (Thread *)pthis;
    // Set up the thread index inside our thread entry
    thread_index_local = obj->_thread_index;
    
    // Calling the real thread function
    obj->_thread_func(obj->_thread_arg);
    return NULL;
  }

  int spawn(pthread_t *tid, const pthread_attr_t *attr, ThreadFunction *fn, void *arg, int thread_index) {

    // Hook up the thread function and argumens pointer and register thread id
    _thread_func = fn;
    _thread_arg = arg;
    _thread_index = thread_index;
    
    DEBUG("Call real pthread_create");
    // Pack the function/arugment pointer in thread object.Then pass it via the 4th argument
    return Pthread::getInstance().create(tid, attr, &fakeEntry, this);
  }

  static int join(pthread_t tid, void **val) {
    DEBUG("Call real pthread_join");
    return Pthread::getInstance().join(tid, val);
  }
};

