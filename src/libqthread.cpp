#include "qthread.h"
#include "thread_param.h"
#include "debug.h"


// The parameter we need the thread to have. All threads being spawned share one copy
ThreadParam thread_param;

// Used to keep the safety of parameters passed to each spawned thread 
pthread_mutex_t spawn_lock;



/**
 * Fake entry point of thread. We use it to unregister thread inside the thread body
 */
void * ThreadFuncWrapper(void * param) {

  ThreadParam * obj = static_cast<ThreadParam *>(param);
  
  // Dump parameters
  my_tid = obj->index;
  Func * my_func = obj->func;
  void * my_arg = (void *) obj->arg;

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

  // This API let thread quit the token passing game at any given time
  // For example, if the main process doesn't acquire locks at all, it can call this API
  // and let others go on to play this game
  void qthread_quit_game(void) {
    Qthread::GetInstance().DeregisterMe();
    return;
  }

  // Actually each thread will automatically join game by default
  // But we can call pairs of quit/join game, let a thread temporarily leave game
  void qthread_join_game(void) {
    Qthread::GetInstance().RegisterMe();
    return;
  }

  // pthread basic
  int pthread_create(pthread_t * tid, const pthread_attr_t * attr, ThreadFunction func, void * arg) {
    
    // Register before spawning
    size_t index = Qthread::GetInstance().RegisterMe();

    // This lock is just for the safety of 'thread_param' (global variable).
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

  // FIXME: Here I return my_tid for the convenience of tesing. 
  // Replace it with original pthread_self() call in the release version
  pthread_t pthread_self(void) {
    //return ppthread_self();
    return my_tid;
  }

  int pthread_cancel(pthread_t tid) {
    return ppthread_cancel(tid);
  }

  int pthread_join(pthread_t tid, void ** val) {
    return ppthread_join(tid, val);
  }

  int pthread_exit(void * value_ptr) {
    Qthread::GetInstance().DeregisterMe();
    return ppthread_exit(value_ptr);
  }

  // pthread mutex
  int pthread_mutex_init(pthread_mutex_t * mutex, const pthread_mutexattr_t  *attr) {
    return Qthread::GetInstance().MutexInit(mutex, attr);
  }

  int pthread_mutex_lock(pthread_mutex_t * mutex) {
    return Qthread::GetInstance().MutexLock(mutex);
  }
  
  // Provide a novel api to support "wait" version mutex lock
  int pthread_mutex_waitlock(pthread_mutex_t * mutex) {
    return Qthread::GetInstance().MutexWaitLock(mutex);
  }  

  int pthread_mutex_unlock(pthread_mutex_t * mutex) {
    return Qthread::GetInstance().MutexUnlock(mutex);
  }

  int pthread_mutex_trylock(pthread_mutex_t * mutex) {
    return Qthread::GetInstance().MutexTrylock(mutex);
  }

  int pthread_mutex_destory(pthread_mutex_t * mutex) {
    return Qthread::GetInstance().MutexDestroy(mutex);  
  }

  // pthread spinlock
  int pthread_spin_init(pthread_spinlock_t * spinner, int shared) {
    return Qthread::GetInstance().SpinInit(spinner, shared);
  }

  int pthread_spin_lock(pthread_spinlock_t * spinner) {
    return Qthread::GetInstance().SpinLock(spinner);
  }

  int pthread_spin_unlock(pthread_spinlock_t * spinner) {
    return Qthread::GetInstance().SpinUnlock(spinner);
  }

  int pthread_spin_trylock(pthread_spinlock_t * spinner) {
    return Qthread::GetInstance().SpinTrylock(spinner);
  }

  int pthread_spin_destroy(pthread_spinlock_t * spinner) {
    return Qthread::GetInstance().SpinDestroy(spinner);
  }

  // pthread rwlock
  int pthread_rwlock_init(pthread_rwlock_t * rwlock, const pthread_rwlockattr_t * attr) {
    return Qthread::GetInstance().RwLockInit(rwlock, attr);
  }

  int pthread_rwlock_rdlock(pthread_rwlock_t * rwlock) {
    return Qthread::GetInstance().RdLock(rwlock);
  }

  int pthread_rwlock_wrlock(pthread_rwlock_t * rwlock) {
    return Qthread::GetInstance().WrLock(rwlock);
  }

  int pthread_rwlock_tryrdlock(pthread_rwlock_t * rwlock) {
    return Qthread::GetInstance().TryRdLock(rwlock);
  }

  int pthread_rwlock_trywrlock(pthread_rwlock_t * rwlock) {
    return Qthread::GetInstance().TryWrLock(rwlock);
  }

  int pthread_rwlock_unlock(pthread_rwlock_t * rwlock) {
    return Qthread::GetInstance().RwUnLock(rwlock);
  }

  int pthread_rwlock_destroy(pthread_rwlock_t * rwlock) {
    return Qthread::GetInstance().RwLockDestroy(rwlock);
  }

  // pthread condition variables
  int pthread_cond_init(pthread_cond_t * cond, const pthread_condattr_t * attr) {
    return Qthread::GetInstance().CondInit(cond, attr);
  }

  int pthread_cond_wait(pthread_cond_t * cond, pthread_mutex_t * mutex) {
    return Qthread::GetInstance().CondWait(cond, mutex);
  }

  int pthread_cond_signal(pthread_cond_t * cond) {
    return Qthread::GetInstance().CondSignal(cond);  
  }

  int pthread_cond_broadcast(pthread_cond_t * cond) {
    return Qthread::GetInstance().CondBroadcast(cond);  
  }

  int pthread_cond_destroy(pthread_cond_t * cond) {
    return Qthread::GetInstance().CondDestroy(cond);
  }

  // pthread barrier
  int pthread_barrier_init(pthread_barrier_t * barrier, const pthread_barrierattr_t * attr, unsigned int count) {
    return Qthread::GetInstance().BarrierInit(barrier, attr, count);
  }

  int pthread_barrier_wait(pthread_barrier_t * barrier) {
    return Qthread::GetInstance().BarrierWait(barrier);
  }

  int pthread_barrier_destroy(pthread_barrier_t * barrier) {
    return Qthread::GetInstance().BarrierDestroy(barrier);
  }

}



