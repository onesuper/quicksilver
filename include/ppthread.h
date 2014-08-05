#pragma once

#include <sys/types.h>
#include <dlfcn.h>
#include <assert.h>

// Pthread thread body
typedef void *ThreadFunction(void *);

// Defining global function pointers for calling real pthread functions

// pthread basic
int (*ppthread_create)(pthread_t *, const pthread_attr_t *, ThreadFunction *, void *);
int (*ppthread_cancel)(pthread_t);
int (*ppthread_join)(pthread_t, void **);
int (*ppthread_exit)(void *);

// pthread mutexes
int (*ppthread_mutex_init)(pthread_mutex_t *, const pthread_mutexattr_t *);
int (*ppthread_mutex_lock)(pthread_mutex_t *);
int (*ppthread_mutex_unlock)(pthread_mutex_t *);
int (*ppthread_mutex_trylock)(pthread_mutex_t *);
int (*ppthread_mutex_destroy)(pthread_mutex_t *);

// pthread mutexes
int (*ppthread_cond_init)(pthread_cond_t *, const pthread_condattr_t *);
int (*ppthread_cond_wait)(pthread_cond_t *, pthread_mutex_t *);
int (*ppthread_cond_signal)(pthread_cond_t *);
int (*ppthread_cond_broadcast)(pthread_cond_t *);
int (*ppthread_cond_destroy)(pthread_cond_t *);

// pthread barriers
int (*ppthread_barrier_init)(pthread_barrier_t *, const pthread_barrierattr_t *, unsigned int);
int (*ppthread_barrier_wait)(pthread_barrier_t *);
int (*ppthread_barrier_destroy)(pthread_barrier_t *);



#define LOAD_SYM(name, handle) \
  p##name = (typeof(p##name)) dlsym(handle, #name); \
  assert(p##name != NULL);


void init_pthread_reference() {

  void *pthread_handle = dlopen("libpthread.so.0", RTLD_NOW);
  
  if (pthread_handle == NULL) {
    DEBUG("%s", dlerror());
    assert(0);
  }

	LOAD_SYM(pthread_create, pthread_handle);
	LOAD_SYM(pthread_cancel, pthread_handle);
  LOAD_SYM(pthread_exit, pthread_handle);
  LOAD_SYM(pthread_join, pthread_handle);
  
  LOAD_SYM(pthread_mutex_init, pthread_handle);
  LOAD_SYM(pthread_mutex_lock, pthread_handle);
  LOAD_SYM(pthread_mutex_unlock, pthread_handle);
  LOAD_SYM(pthread_mutex_trylock, pthread_handle);
  LOAD_SYM(pthread_mutex_destroy, pthread_handle);
  
  LOAD_SYM(pthread_cond_init, pthread_handle);
  LOAD_SYM(pthread_cond_wait, pthread_handle);
  LOAD_SYM(pthread_cond_signal, pthread_handle);
  LOAD_SYM(pthread_cond_broadcast, pthread_handle);
  LOAD_SYM(pthread_cond_destroy, pthread_handle);
  
  LOAD_SYM(pthread_barrier_init, pthread_handle);
  LOAD_SYM(pthread_barrier_wait, pthread_handle);
  LOAD_SYM(pthread_barrier_destroy, pthread_handle);

  //dlclose(pthread_handle);
}


    

