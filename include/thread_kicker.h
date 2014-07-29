#pragma once


#include "qthread.h"
#include "ppthread.h"
#include "thread_private.h"

// Thread local storage: use to let thread know who he is
__thread size_t thread_index_local;
  

/**
 * This class is used to spawn a thread which is wrapped around our
 * register/deregister functions
 */
class ThreadKicker {

private:

  ThreadFunction *_thread_func;
  void *_thread_arg;
  size_t _thread_index;

public:

  ThreadKicker():
    _thread_func(NULL),
    _thread_arg(NULL),
    _thread_index(0)
  {}
  

  // Fake entry point of thread
  static void *threadEntry(void *pthis) {
  
    ThreadKicker *obj = (ThreadKicker *)pthis;
   
    // Set up the thread index in the thread's lifecycle
    set_my_tid(obj->_thread_index);

    // Calling the real thread function
    obj->_thread_func(obj->_thread_arg);

    // deregister me!
    Qthread::getInstance().deregisterThread(obj->_thread_index);

    return NULL;
  }

   int spawn(pthread_t *tid, const pthread_attr_t *attr, ThreadFunction fn, void *arg) {


    static int global_thread_count = 0;

    Qthread::getInstance().lock();
    
    // Hook up the thread function and argumens pointer and store thread id
    _thread_index = global_thread_count++;
    Qthread::getInstance().unlock();
    
    _thread_func = fn;
    _thread_arg = arg;

    // Register before spawning
    Qthread::getInstance().registerThread(_thread_index);

    DEBUG("Spawning thread %ld ...", _thread_index);
    DEBUG("Call real pthread_create");
    
    // Pack the function/arugment pointer in thread object.Then pass it via the 4th argument
    int retval = Pthread::getInstance().create(tid, attr, &threadEntry, this);
 

    return retval;
  }

};

