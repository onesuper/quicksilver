#pragma once


#include "pthread.h"  // our pthread header
#include "thread.h"
#include "list.h"


class Qthread {


private:

  // Register all the active threads into this list.
  List _activelist;

  // All threads use this lock to prevent from data race.
  pthread_mutex_t        _mutex;
  pthread_mutexattr_t    _mutexattr;

  // Pointing to the thread entry who holds the token
  ThreadEntry *_token_entry;

  inline void lock(void) {
    Pthread::getInstance().mutex_lock(&_mutex);
  }

  inline void unlock(void) {
    Pthread::getInstance().mutex_unlock(&_mutex);
  }

  void wait_for_token(void) {

  }

  void pass_token(void) {

  }

public:

  Qthread():
    _token_entry(NULL)
    {}

  // Singleton pattern
  static Qthread& getInstance(void) {
    static Qthread *obj = NULL;
    if (obj == NULL) { 
      obj = new Qthread();
    }
    return *obj;
  }

  void init() {
    Pthread::getInstance().init();
    Pthread::getInstance().mutexattr_init(&_mutexattr);
    Pthread::getInstance().mutex_init(&_mutex, &_mutexattr);
  }

  int create(pthread_t *tid, const pthread_attr_t *attr, ThreadFunction *fn , void *arg) {

    int thread_index;

    // Use a static variable to cout the number of threads
    static size_t thread_count = 0;

    lock();
    thread_index = thread_count++;
    unlock();

    // Create thread entry and add it to the _activelist
    ThreadEntry *entry = new ThreadEntry(thread_index);
    _activelist.insertTail(entry);
    _activelist.print();

    // At the very beginning, token belongs to the first created thread
    if (_token_entry == NULL) {
      _token_entry = entry;
    }

    DEBUG("Who has the token?");
    _token_entry->print();

    // Start spawning thread with the thread_index
    DEBUG("Spawning thread %d", thread_index);
    Thread* t = new Thread();
    return t->spawn(tid, attr, fn, arg, thread_index);
  }

  int cancel(pthread_t tid) {
    DEBUG("Call real pthread_cancel");
    return Pthread::getInstance().cancel(tid);
  }

  int join(pthread_t tid, void **val) {

    //lock();
    // remove the thread entry
    //_activelist.remove();
    //unlock();
    return Thread::join(tid, val);
  }

  int exit(void *val_ptr) {
    DEBUG("Call real pthread_exit");
    return Pthread::getInstance()._exit(val_ptr);
  }

  int mutexattr_init(pthread_mutexattr_t *attr) {
    DEBUG("Call real pthread_mutexattr_init");
    return Pthread::getInstance().mutexattr_init(attr);
  }

  int mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    DEBUG("Call real pthread_mutex_init");
    return Pthread::getInstance().mutex_init(mutex, attr);
  }

  int mutex_lock(pthread_mutex_t *mutex) {
    DEBUG("[%d] Call real pthread_mutex_lock", Thread::getIndex());
    return Pthread::getInstance().mutex_lock(mutex);
  }

  int mutex_unlock(pthread_mutex_t *mutex) {
    DEBUG("[%d] Call real pthread_mutex_unlock", Thread::getIndex());
    return Pthread::getInstance().mutex_unlock(mutex);
  }

  int mutex_trylock(pthread_mutex_t *mutex) {
    DEBUG("Call real pthread_mutex_trylock");
    return Pthread::getInstance().mutex_trylock(mutex);
  }

  int mutex_destroy(pthread_mutex_t *mutex) {
    DEBUG("Call real pthread_mutex_destroy");
    return Pthread::getInstance().mutex_destroy(mutex);
  }

  int condattr_init(pthread_condattr_t *attr) {
    DEBUG("Call real pthread_condattr_init");
    return Pthread::getInstance().condattr_init(attr);
  }

  int cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    DEBUG("Call real pthread_cond_init");
    return Pthread::getInstance().cond_init(cond, attr);
  }

  int cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    DEBUG("Call real pthread_cond_wait");
    return Pthread::getInstance().cond_wait(cond, mutex);
  }

  int cond_signal(pthread_cond_t *cond) {
    DEBUG("Call real pthread_cond_signal");
    return Pthread::getInstance().cond_signal(cond);
  }

  int cond_broadcast(pthread_cond_t *cond) {
    DEBUG("Call real pthread_cond_broadcast");
    return Pthread::getInstance().cond_broadcast(cond);
  }

  int cond_destroy(pthread_cond_t *cond) {
    DEBUG("Call real pthread_cond_destroy");
    return Pthread::getInstance().cond_destroy(cond);
  }

  int barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count) {
    DEBUG("Call real pthread_barrier_init");
    return Pthread::getInstance().barrier_init(barrier, attr, count);
  }

  int barrier_wait(pthread_barrier_t *barrier) {
    DEBUG("Call real pthread_barrier_wait");
    return Pthread::getInstance().barrier_wait(barrier);
  }

  int barrier_destroy(pthread_barrier_t *barrier) {
    DEBUG("Call real pthread_barrier_destroy");
    return Pthread::getInstance().barrier_destroy(barrier);
  }

  void del() {
    Pthread::getInstance().del();
  }

  
};
