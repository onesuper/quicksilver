
#include "debug.h"


/**
 * 
 */

// GCC-specific global constructor
#if defined(__GNUG__)
void before_everything() __attribute__((constructor));
void after_everything() __attribute__((destructor));
#endif


// Call them to init Pthread class before or after main
void before_everything() {
  Qthread::getInstance().init();
}

void after_everyting() {
  Qthread::getInstance().del();
}

// pthread basic
int pthread_create (pthread_t *tid, const pthread_attr_t *attr, void *(*fn) (void *), void *arg) {
  DEBUG("Call replaced pthread_create");
  return Qthread::getInstance().create(tid, attr, fn, arg);
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
  DEBUG("Call replaced pthread_exit");
  return Qthread::getInstance().exit(ptr);
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
  DEBUG("Call replaced pthread_mutex_lock");
  return Qthread::getInstance().mutex_lock(mutex);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
  DEBUG("Call replaced pthread_mutex_unlock");
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

int pthread_cond_destroy(pthread_cond_t * cond) {
  DEBUG("Call replaced pthread_cond_destroy");
  return Qthread::getInstance().cond_destroy(cond);
}

// pthread barrier
int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t * attr, unsigned int count) {
  DEBUG("Call replaced pthread_barrier_init");
  return Qthread::getInstance().barrier_init(barrier, attr, count);
}

int pthread_barrier_wait(pthread_barrier_t *barrier) {
  DEBUG("Call replaced pthread_barrier_wait");
  return Qthread::getInstance().barrier_wait(barrier);
}

int pthread_barrier_destroy(pthread_barrier_t *barrier) {
  DEBUG("Call replaced pthread_barrier_destroy");
  return Qthread::getInstance().barrier_destroy();
}

