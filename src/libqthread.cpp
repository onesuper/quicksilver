#include "qthread.h"
#include "thread_param.h"
#include "debug.h"


/**
 * Globals
 */

// The parameter we need the thread to have. All threads being spawned share one copy
ThreadParam thread_param;

// Used to keep the safety of parameters passed to each spawned thread 
pthread_mutex_t spawn_lock;

// Define the only Qthread instance
Qthread Qthread::_instance;

// Count the assigned thread id
static volatile size_t thread_count = 0;

// Replace pthread_self
//extern size_t my_tid;

/**
 * Fake entry point of thread. We use it to unregister thread inside the thread body
 */
void *ThreadFuncWrapper(void *param) {

  ThreadParam *obj = static_cast<ThreadParam *>(param);
  
  // Dump parameters
  my_tid = obj->index;
  Func *my_func = obj->func;
  void *my_arg = (void *) obj->arg;

  // Unlock after copying out paramemters
  ppthread_mutex_unlock(&spawn_lock);

  // Call the real thread function
  void* retval = my_func(my_arg);

  // Let each thread deregister it self
  Qthread::GetInstance().DeregisterMe();

  return retval;
}


/**
 * Shims to call replaced pthread subroutines
 */
extern "C" {
  // pthread basic
  int pthread_create(pthread_t *tid, const pthread_attr_t *attr, ThreadFunction func, void *arg) {
    
    size_t index;
    
    // Use gcc atomic operation to increment id
    index = __sync_fetch_and_add(&thread_count, 1);    

    DEBUG("Spawning thread %ld", index);

    // Register before spawning
    Qthread::GetInstance().RegisterThread(index);

    // This lock is for the safety of thread_param
    // TODO: this lock will make the initialization of threads be *serialized*.
    // When we have a lot of threads to spawn, that may hurt performance.
    // Try to replace it with a better mechanism later.
    ppthread_mutex_lock(&spawn_lock);

    // Hook up the thread function and arguments
    thread_param.index = index;
    thread_param.func = func;
    thread_param.arg = arg;

    // The unlock of spawn_lock is located in the ThreadFuncWrapper we passed to ppthread_create()
    int retval = ppthread_create(tid, attr, ThreadFuncWrapper, &thread_param);

    return retval;
  }

  pthread_t pthread_self(void) {
    //return ppthread_self();
    return my_tid;
  }

  int pthread_cancel(pthread_t tid) {
    DEBUG("call pthread_cancel");
    return ppthread_cancel(tid);
  }

  int pthread_join(pthread_t tid, void **val) {
    return ppthread_join(tid, val);
  }

  int pthread_exit(void *value_ptr) {
    DEBUG("<%lu> call pthread_exit", my_tid);
    Qthread::GetInstance().DeregisterMe();
    return ppthread_exit(value_ptr);
  }

  // pthread mutex
  int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    return Qthread::GetInstance().MutexInit(mutex, attr);
  }

  int pthread_mutex_lock(pthread_mutex_t *mutex) {
    return Qthread::GetInstance().MutexLock(mutex);
  }
  
  // Provide a novel api to support "wait" version mutex lock
  int pthread_mutex_waitlock(pthread_mutex_t *mutex) {
    return Qthread::GetInstance().MutexWaitLock(mutex);
  }  

  int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    return Qthread::GetInstance().MutexUnlock(mutex);
  }

  int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    return Qthread::GetInstance().MutexTrylock(mutex);
  }

  int pthread_mutex_destory(pthread_mutex_t *mutex) {
    return Qthread::GetInstance().MutexDestroy(mutex);  
  }

  // pthread spin
  int pthread_spin_init(pthread_spinlock_t *spinner, int shared) {
    return Qthread::GetInstance().SpinInit(spinner, shared);
  }

  int pthread_spin_lock(pthread_spinlock_t *spinner) {
    return Qthread::GetInstance().SpinLock(spinner);
  }

  int pthread_spin_unlock(pthread_spinlock_t *spinner) {
    return Qthread::GetInstance().SpinUnlock(spinner);
  }

  int pthread_spin_trylock(pthread_spinlock_t *spinner) {
    return Qthread::GetInstance().SpinTrylock(spinner);
  }

  int pthread_spin_destroy(pthread_spinlock_t *spinner) {
    return Qthread::GetInstance().SpinDestroy(spinner);
  }

  // pthread condition variables
  int pthread_cond_init(pthread_cond_t * cond, const pthread_condattr_t *attr) {
    return Qthread::GetInstance().CondInit(cond, attr);
  }

  int pthread_cond_wait(pthread_cond_t * cond, pthread_mutex_t *mutex) {
    return Qthread::GetInstance().CondWait(cond, mutex);
  }

  int pthread_cond_signal(pthread_cond_t *cond) {
    return Qthread::GetInstance().CondSignal(cond);  
  }

  int pthread_cond_broadcast(pthread_cond_t *cond) {
    return Qthread::GetInstance().CondBroadcast(cond);  
  }

  int pthread_cond_destroy(pthread_cond_t *cond) {
    return Qthread::GetInstance().CondDestroy(cond);
  }

  // pthread barrier
  int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count) {
    return Qthread::GetInstance().BarrierInit(barrier, attr, count);
  }

  int pthread_barrier_wait(pthread_barrier_t *barrier) {
    return Qthread::GetInstance().BarrierWait(barrier);
  }

  int pthread_barrier_destroy(pthread_barrier_t *barrier) {
    return Qthread::GetInstance().BarrierDestroy(barrier);
  }

}



