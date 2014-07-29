
#include "stdlib.h"
#include "debug.h"
#include "qthread.h"
#include "thread_kicker.h"
#include "thread_private.h"

/**
 * A shim to call replaced pthread subroutines
 */

// GCC-specific global constructor
// Call them to init Pthread class before or after main

#if defined(__GNUG__)

__attribute__((constructor))
void before_everything() {
  DEBUG("Registering qthread instance");
  Qthread::getInstance().init();
}

__attribute__((destructor))
void after_everyting() {
  DEBUG("Deregistering qthread instance");
  Qthread::getInstance().del();
}

#endif


extern "C" {

// pthread basic
int pthread_create (pthread_t *tid, const pthread_attr_t *attr, ThreadFunction fn, void *arg) {
  DEBUG("Call replaced pthread_create");
  ThreadKicker *kicker = new ThreadKicker();
  return kicker->spawn(tid, attr, fn, arg);
}

int pthread_cancel(pthread_t tid) {
  DEBUG("Call replaced pthread_cancel");
  return Qthread::getInstance().cancel(tid);
}

int pthread_join(pthread_t tid, void **val) {
  DEBUG("Call replaced pthread_join");
  return Qthread::getInstance().join(tid, val);
}

int pthread_exit(void *value_ptr) {
  DEBUG("<%d> call replaced pthread_exit", my_tid());
  return Qthread::getInstance().exit(value_ptr);
}

// pthread mutex
int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
  DEBUG("Call replaced pthread_mutexattr_init");
  return Qthread::getInstance().mutexattr_init(attr);
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
  DEBUG("Call replaced pthread_mutex_init");
  return Qthread::getInstance().mutex_init(mutex, attr);
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
  DEBUG("<%d> call replaced pthread_mutex_lock", my_tid());
  return Qthread::getInstance().mutex_lock(mutex);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
  DEBUG("<%d> all replaced pthread_mutex_unlock", my_tid());
  return Qthread::getInstance().mutex_unlock(mutex);
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
  DEBUG("Call replaced pthread_mutex_trylock");
  return Qthread::getInstance().mutex_trylock(mutex);
}

int pthread_mutex_destory(pthread_mutex_t *mutex) {
  DEBUG("Call replaced pthread_mutex_destroy");
  return Qthread::getInstance().mutex_destroy(mutex);  
}

// pthread condition variables
int pthread_condattr_init(pthread_condattr_t *attr) {
  DEBUG("Call replaced pthread_condattr_init");
  return Qthread::getInstance().condattr_init(attr);
}

int pthread_cond_init(pthread_cond_t * cond, const pthread_condattr_t *attr) {
  DEBUG("Call replaced pthread_cond_init");
  return Qthread::getInstance().cond_init(cond, attr);
}

int pthread_cond_wait(pthread_cond_t * cond, pthread_mutex_t *mutex) {
  DEBUG("Call replaced pthread_cond_wait");
  return Qthread::getInstance().cond_wait(cond, mutex);
}

int pthread_cond_signal(pthread_cond_t *cond) {
  DEBUG("Call replaced pthread_cond_signal");
  return Qthread::getInstance().cond_signal(cond);  
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
  DEBUG("Call replaced pthread_cond_broadcast");
  return Qthread::getInstance().cond_broadcast(cond);  
}

int pthread_cond_destroy(pthread_cond_t *cond) {
  DEBUG("Call replaced pthread_cond_destroy");
  return Qthread::getInstance().cond_destroy(cond);
}

// pthread barrier
int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count) {
  DEBUG("Call replaced pthread_barrier_init");
  return Qthread::getInstance().barrier_init(barrier, attr, count);
}

int pthread_barrier_wait(pthread_barrier_t *barrier) {
  DEBUG("Call replaced pthread_barrier_wait");
  return Qthread::getInstance().barrier_wait(barrier);
}

int pthread_barrier_destroy(pthread_barrier_t *barrier) {
  DEBUG("Call replaced pthread_barrier_destroy");
  return Qthread::getInstance().barrier_destroy(barrier);
}

}


