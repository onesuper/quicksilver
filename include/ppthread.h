#ifndef _PPTHREAD_H_
#define _PPTHREAD_H_

#include <sys/types.h>
#include <dlfcn.h>
#include <time.h>
#include <assert.h>

// Pthread thread body
typedef void *ThreadFunction(void *);


/**
 * Defining global function pointers for calling real pthread functions
 */
// pthread basic
int (*ppthread_create)(pthread_t *, const pthread_attr_t *, ThreadFunction *, void *);
int (*ppthread_cancel)(pthread_t);
int (*ppthread_join)(pthread_t, void **);
int (*ppthread_exit)(void *);
pthread_t (*ppthread_self)(void);

// pthread mutexes
int (*ppthread_mutex_init)(pthread_mutex_t *, const pthread_mutexattr_t *);
int (*ppthread_mutex_lock)(pthread_mutex_t *);
int (*ppthread_mutex_unlock)(pthread_mutex_t *);
int (*ppthread_mutex_trylock)(pthread_mutex_t *);
int (*ppthread_mutex_destroy)(pthread_mutex_t *);

// pthread spin
int (*ppthread_spin_init)(pthread_spinlock_t *, int);
int (*ppthread_spin_lock)(pthread_spinlock_t *);
int (*ppthread_spin_unlock)(pthread_spinlock_t *);
int (*ppthread_spin_trylock)(pthread_spinlock_t *);
int (*ppthread_spin_destroy)(pthread_spinlock_t *);

// pthread rwlock
int (*ppthread_rwlock_init)(pthread_rwlock_t *, const pthread_rwlockattr_t *);
int (*ppthread_rwlock_rdlock)(pthread_rwlock_t *);
int (*ppthread_rwlock_wrlock)(pthread_rwlock_t *);
int (*ppthread_rwlock_timedrdlock)(pthread_rwlock_t *, const struct timespec *);
int (*ppthread_rwlock_timedwrlock)(pthread_rwlock_t *, const struct timespec *);
int (*ppthread_rwlock_tryrdlock)(pthread_rwlock_t *);
int (*ppthread_rwlock_trywrlock)(pthread_rwlock_t *);
int (*ppthread_rwlock_unlock)(pthread_rwlock_t *);
int (*ppthread_rwlock_destroy)(pthread_rwlock_t *);


// pthread cond
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
    //DEBUG("%s", dlerror());
    assert(0);
  }

  LOAD_SYM(pthread_create, pthread_handle);
  LOAD_SYM(pthread_cancel, pthread_handle);
  LOAD_SYM(pthread_exit, pthread_handle);
  LOAD_SYM(pthread_join, pthread_handle);
  LOAD_SYM(pthread_self, pthread_handle);  

  LOAD_SYM(pthread_mutex_init, pthread_handle);
  LOAD_SYM(pthread_mutex_lock, pthread_handle);
  LOAD_SYM(pthread_mutex_unlock, pthread_handle);
  LOAD_SYM(pthread_mutex_trylock, pthread_handle);
  LOAD_SYM(pthread_mutex_destroy, pthread_handle);
  
  LOAD_SYM(pthread_spin_init, pthread_handle);
  LOAD_SYM(pthread_spin_lock, pthread_handle);
  LOAD_SYM(pthread_spin_unlock, pthread_handle);
  LOAD_SYM(pthread_spin_trylock, pthread_handle);
  LOAD_SYM(pthread_spin_destroy, pthread_handle);

  LOAD_SYM(pthread_rwlock_init, pthread_handle);
  LOAD_SYM(pthread_rwlock_rdlock, pthread_handle);
  LOAD_SYM(pthread_rwlock_wrlock, pthread_handle);
  LOAD_SYM(pthread_rwlock_timedrdlock, pthread_handle);
  LOAD_SYM(pthread_rwlock_timedwrlock, pthread_handle);
  LOAD_SYM(pthread_rwlock_tryrdlock, pthread_handle);
  LOAD_SYM(pthread_rwlock_trywrlock, pthread_handle);
  LOAD_SYM(pthread_rwlock_unlock, pthread_handle);  
  LOAD_SYM(pthread_rwlock_destroy, pthread_handle);

  LOAD_SYM(pthread_cond_init, pthread_handle);
  LOAD_SYM(pthread_cond_wait, pthread_handle);
  LOAD_SYM(pthread_cond_signal, pthread_handle);
  LOAD_SYM(pthread_cond_broadcast, pthread_handle);
  LOAD_SYM(pthread_cond_destroy, pthread_handle);
  
  LOAD_SYM(pthread_barrier_init, pthread_handle);
  LOAD_SYM(pthread_barrier_wait, pthread_handle);
  LOAD_SYM(pthread_barrier_destroy, pthread_handle);

  // close after binding
  dlclose(pthread_handle);
}


  
#endif
