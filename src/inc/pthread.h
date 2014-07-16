#pragma once


#include <pthread.h>
#include <dlfcn.h>
#include <assert.h>
#include <stdio.h>
#include "debug.h"



class Pthread {
private:
  
  // File handle to the opened libpthread
  void *_pthread_handle;

  // Some func pointers used to call real pthread libraries
  // pthread basics
  int (*_pthread_create) (pthread_t*, const pthread_attr_t*, void *(*)(void*), void*);
  int (*_pthread_cancel) (pthread_t);
  int (*_pthread_join) (pthread_t, void**);
  int (*_pthread_exit) (void*);

  // pthread mutexes
  int (*_pthread_mutexattr_init) (pthread_mutexattr_t*);
  int (*_pthread_mutex_init) (pthread_mutex_t*, const pthread_mutexattr_t*);
  int (*_pthread_mutex_lock) (pthread_mutex_t*);
  int (*_pthread_mutex_unlock) (pthread_mutex_t*);
  int (*_pthread_mutex_trylock) (pthread_mutex_t*);
  int (*_pthread_mutex_destroy) (pthread_mutex_t*);

   // pthread condition variables
  int (*_pthread_condattr_init) (pthread_condattr_t*);
  int (*_pthread_cond_init) (pthread_cond_t*, pthread_condattr_t*);
  int (*_pthread_cond_wait) (pthread_cond_t*, pthread_mutex_t*);
  int (*_pthread_cond_signal) (pthread_cond_t*);
  int (*_pthread_cond_broadcast) (pthread_cond_t*);
  int (*_pthread_cond_destroy) (pthread_cond_t*);

  // pthread barriers
  int (*_pthread_barrier_init) (pthread_barrier_t*, pthread_barrierattr_t*, unsigned int);
  int (*_pthread_barrier_wait) (pthread_barrier_t*);
  int (*_pthread_barrier_destroy) (pthread_barrier_t*);

public:

  // constructor
  Pthread():
    _pythread_create(NULL),
    _pthread_cancel(NULL),
    _pthread_join(NULL),
    _pthread_exit(NULL),
    _pthread_mutexattr_init(NULL),
    _pthread_mutex_init(NULL),
    _pthread_mutex_lock(NULL),
    _pthread_mutex_unlock(NULL),
    _pthread_mutex_trylock(NULL),
    _pthread_mutex_destroy(NULL),
    _pthread_condattr_init(NULL),
    _pthread_cond_init(NULL),
    _pthread_cond_wait(NULL),
    _pthread_cond_signal(NULL),
    _pthread_cond_broadcast(NULL),
    _pthread_cond_destroy(NULL),
    _pthread_barrier_init(NULL),
    _pthread_barrier_wait(NULL),
    _pthread_barrier_destroy(NULL)
    {}

  void init() {
    DEBUG("Initializing references to replaced functions inlibpthread");

    // Open the libpthread
    _pthread_handle = dlopen("libpthread.so.0", RTLD_NOW | RTLD_GLOBAL | RTLD_NOLOAD);
    if (pthread_handle == NULL) {
      fprintf(stderr, "Unable to load libpthread.so.0\n");
      fprintf(stderr, dlerror());
      fprintf(stderr, "\n")
      return;
    }

    // Bind pthread calls to our own references
    #define LOAD_SYM(name, handle) \
          _##name = (typeof(_##name) dlsym(handle, #name); \
          assert(_##name != NULL);

    LOAD_SYM(pthread_create, pthread_handle);
    LOAD_SYM(pthread_cancel, pthread_handle);
    LOAD_SYM(pthread_join, pthread_handle);
    LOAD_SYM(pthread_exit, pthread_handle);
    LOAD_SYM(pthread_mutex_init, pthread_handle);
    LOAD_SYM(pthread_mutex_lock, pthread_handle);
    LOAD_SYM(pthread_mutex_unlock, pthread_handle);
    LOAD_SYM(pthread_mutex_trylock, pthread_handle);
    LOAD_SYM(pthread_mutex_destroy, pthread_handle);
    LOAD_SYM(pthread_mutexattr_init, pthread_handle);
    LOAD_SYM(pthread_condattr_init, pthread_handle);
    LOAD_SYM(pthread_cond_init, pthread_handle);
    LOAD_SYM(pthread_cond_wait, pthread_handle);
    LOAD_SYM(pthread_cond_signal, pthread_handle);
    LOAD_SYM(pthread_cond_broadcast, pthread_handle);
    LOAD_SYM(pthread_cond_destroy, pthread_handle);
    LOAD_SYM(pthread_barrier_init, pthread_handle);
    LOAD_SYM(pthread_barrier_wait, pthread_handle);
    LOAD_SYM(pthread_barrier_destroy, pthread_handle);
  }


  void del() {
    if (_pthread_handle != NULL) {
      DEBUG("Unload the libpthread");
      dlclose(_pthread_handle);
    }

    _pthread_handle = NULL;
    _pthread_create = NULL;
    _pthread_cancel = NULL;
    _pthread_join = NULL;
    _pthread_exit = NULL;
    _pthread_mutexattr_init = NULL;
    _pthread_mutex_init = NULL;
    _pthread_mutex_lock = NULL;
    _pthread_mutex_unlock = NULL;
    _pthread_mutex_trylock = NULL;
    _pthread_mutex_destroy = NULL;
    _pthread_condattr_init = NULL;
    _pthread_cond_init = NULL;
    _pthread_cond_wait = NULL;
    _pthread_cond_signal = NULL;
    _pthread_cond_broadcast = NULL;
    _pthread_cond_destroy = NULL;
    _pthread_barrier_init = NULL;
    _pthread_barrier_wait NULL;
    _pthread_barrier_destroy = NULL;

  }
}