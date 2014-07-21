#pragma once


#include <sys/types.h>
#include <dlfcn.h>
#include <assert.h>
#include <stdio.h>
#include "debug.h"
#include "error.h"


/**
 * Provide an interface for calling real pthread functions.
 * The Pthread class has only one instance, which is initialized when first use
 */
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
  int (*_pthread_cond_init) (pthread_cond_t*, const pthread_condattr_t*);
  int (*_pthread_cond_wait) (pthread_cond_t*, pthread_mutex_t*);
  int (*_pthread_cond_signal) (pthread_cond_t*);
  int (*_pthread_cond_broadcast) (pthread_cond_t*);
  int (*_pthread_cond_destroy) (pthread_cond_t*);

  // pthread barriers
  int (*_pthread_barrier_init) (pthread_barrier_t*, const pthread_barrierattr_t*, unsigned int);
  int (*_pthread_barrier_wait) (pthread_barrier_t*);
  int (*_pthread_barrier_destroy) (pthread_barrier_t*);



public:
 // constructor
  Pthread():
    _pthread_create(NULL),
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

    DEBUG("Initializing references to real functions in libpthread");
    DEBUG("dlopen libthread");

    _pthread_handle = dlopen("libpthread.so.0", RTLD_NOW);
    if (_pthread_handle == NULL) {
      ERROR("Unable to load libpthread.so.0: %s", dlerror());
      ERROR("Aborting...");
      exit(1);
    }

    // Bind pthread calls to our own references
#define LOAD_SYM(name) \
          _##name = (typeof(_##name)) dlsym(_pthread_handle, #name); \
          assert(_##name != NULL);

    LOAD_SYM(pthread_create);
    LOAD_SYM(pthread_cancel);
    LOAD_SYM(pthread_join);
    LOAD_SYM(pthread_exit);
    LOAD_SYM(pthread_mutex_init);
    LOAD_SYM(pthread_mutex_lock);
    LOAD_SYM(pthread_mutex_unlock);
    LOAD_SYM(pthread_mutex_trylock);
    LOAD_SYM(pthread_mutex_destroy);
    LOAD_SYM(pthread_mutexattr_init);
    LOAD_SYM(pthread_condattr_init);
    LOAD_SYM(pthread_cond_init);
    LOAD_SYM(pthread_cond_wait);
    LOAD_SYM(pthread_cond_signal);
    LOAD_SYM(pthread_cond_broadcast);
    LOAD_SYM(pthread_cond_destroy);
    LOAD_SYM(pthread_barrier_init);
    LOAD_SYM(pthread_barrier_wait);
    LOAD_SYM(pthread_barrier_destroy);

  }


  // Singleton pattern
  static Pthread& getInstance(void) {
    static Pthread *obj = NULL;
    if (obj == NULL) { 
      obj = new Pthread();
      //obj.init();  initialize automatically
    }
    return *obj;
  }


  int create(pthread_t *tid, const pthread_attr_t *attr, void *(*fn)(void *) , void *arg) {
    return _pthread_create(tid, attr, fn, arg);
  }

  int cancel(pthread_t tid) {
    return _pthread_cancel(tid);
  }

  int join(pthread_t tid, void **val) {
    return _pthread_join(tid, val);
  }

  int exit(void *val_ptr) {
    return _pthread_exit(val_ptr);
  }

  int mutexattr_init(pthread_mutexattr_t *attr) {
    return _pthread_mutexattr_init(attr);
  }

  int mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    return _pthread_mutex_init(mutex, attr);
  }

  int mutex_lock(pthread_mutex_t *mutex) {
    return _pthread_mutex_lock(mutex);
  }

  int mutex_unlock(pthread_mutex_t *mutex) {
    return _pthread_mutex_unlock(mutex);
  }

  int mutex_trylock(pthread_mutex_t *mutex) {
    return _pthread_mutex_trylock(mutex);
  }

  int mutex_destroy(pthread_mutex_t *mutex) {
    return _pthread_mutex_destroy(mutex);
  }

  int condattr_init(pthread_condattr_t *attr) {
    return _pthread_condattr_init(attr);
  }

  int cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    return _pthread_cond_init(cond, attr);
  }

  int cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    return _pthread_cond_wait(cond, mutex);
  }

  int cond_signal(pthread_cond_t *cond) {
    return _pthread_cond_signal(cond);
  }

  int cond_broadcast(pthread_cond_t *cond) {
    return _pthread_cond_broadcast(cond);
  }

  int cond_destroy(pthread_cond_t *cond) {
    return _pthread_cond_destroy(cond);
  }

  int barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count) {
    return _pthread_barrier_init(barrier, attr, count);
  }

  int barrier_wait(pthread_barrier_t *barrier) {
    return _pthread_barrier_wait(barrier);
  }

  int barrier_destroy(pthread_barrier_t *barrier) {
    return _pthread_barrier_destroy(barrier);
  }

  void del() {
    if (_pthread_handle != NULL) {
      DEBUG("dlclose libthread");
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
    _pthread_barrier_wait = NULL;
    _pthread_barrier_destroy = NULL;

  }
};
