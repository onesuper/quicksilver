#include "debug.h"
#include "qthread.h"
#include "thread_wrapper.h"



// Global: used to pass thread's parameters when spawning thread
pthread_mutex_t spawn_lock;


 
// GCC-specific global constructor
// Call them to init/del Pthread class before or after main
#if defined(__GNUG__)
 __attribute__((constructor))
 void before_everything() {
 
  DEBUG("Registering pthread instance");
  init_pthread_reference();
  ppthread_mutex_init(&spawn_lock, NULL);
  Qthread::GetInstance().Init();
}

__attribute__((destructor))
void after_everyting() {
  Qthread::GetInstance().Destroy();
}
#endif


/**
 * Shims to call replaced pthread subroutines
 */
extern "C" {
  // pthread basic
  int pthread_create(pthread_t *tid, const pthread_attr_t *attr, ThreadFunction func, void *arg) {
    
    static size_t thread_count = 0;
		volatile size_t index;
		
    // Ensure the safety of thread_param
    ppthread_mutex_lock(&spawn_lock);

    // Use gcc atomic operation to increment id
    //index = __sync_fetch_and_add(&thread_count, 1);    
    index = thread_count++;
    DEBUG("Spawning thread %ld", index);  

    // Hook up the thread function and arguments
    thread_param.index = index;
    thread_param.func = func;
    thread_param.arg = arg;

    // Register before spawning
    Qthread::GetInstance().RegisterThread(index);

    int retval = ppthread_create(tid, attr, ThreadFuncWrapper, &thread_param);     
    return retval;
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
    Qthread::GetInstance().DeregisterThread(my_tid);
    return ppthread_exit(value_ptr);
  }

  // pthread mutex
  int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    DEBUG("Call replaced pthread_mutex_init");
    return Qthread::GetInstance().MutexInit(mutex, attr);
  }

  int pthread_mutex_lock(pthread_mutex_t *mutex) {
    DEBUG("<%lu> call replaced pthread_mutex_lock", my_tid);
    return Qthread::GetInstance().MutexLock(mutex);
  }

  int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    DEBUG("<%lu> call replaced pthread_mutex_unlock", my_tid);
    return Qthread::GetInstance().MutexUnlock(mutex);
  }

  int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    DEBUG("Call replaced pthread_mutex_trylock");
    return Qthread::GetInstance().MutexTrylock(mutex);
  }

  int pthread_mutex_destory(pthread_mutex_t *mutex) {
    DEBUG("Call replaced pthread_mutex_destroy");
    return Qthread::GetInstance().MutexDestroy(mutex);  
  }

  // pthread spin
  int pthread_spin_init(pthread_spinlock_t *spinner, int shared) {
    DEBUG("Call replaced pthread_spin_init");
    return Qthread::GetInstance().SpinInit(spinner, shared);
  }

  int pthread_spin_lock(pthread_spinlock_t *spinner) {
    DEBUG("Call replaced pthread_spin_lock");
    return Qthread::GetInstance().SpinLock(spinner);
  }

  int pthread_spin_unlock(pthread_spinlock_t *spinner) {
    DEBUG("Call replaced pthread_spin_unlock");
    return Qthread::GetInstance().SpinUnlock(spinner);
  }

  int pthread_spin_trylock(pthread_spinlock_t *spinner) {
    DEBUG("Call replaced pthread_spin_trylock");
    return Qthread::GetInstance().SpinTrylock(spinner);
  }

  int pthread_spin_destroy(pthread_spinlock_t *spinner) {
    DEBUG("Call replaced pthread_spin_destroy");
    return Qthread::GetInstance().SpinDestroy(spinner);
  }

  // pthread condition variables
  int pthread_cond_init(pthread_cond_t * cond, const pthread_condattr_t *attr) {
    DEBUG("Call replaced pthread_cond_init");
    return Qthread::GetInstance().CondInit(cond, attr);
  }

  int pthread_cond_wait(pthread_cond_t * cond, pthread_mutex_t *mutex) {
    DEBUG("Call replaced pthread_cond_wait");
    return Qthread::GetInstance().CondWait(cond, mutex);
  }

  int pthread_cond_signal(pthread_cond_t *cond) {
    DEBUG("Call replaced pthread_cond_signal");
    return Qthread::GetInstance().CondSignal(cond);  
  }

  int pthread_cond_broadcast(pthread_cond_t *cond) {
    DEBUG("Call replaced pthread_cond_broadcast");
    return Qthread::GetInstance().CondBroadcast(cond);  
  }

  int pthread_cond_destroy(pthread_cond_t *cond) {
    DEBUG("Call replaced pthread_cond_destroy");
    return Qthread::GetInstance().CondDestroy(cond);
  }

  // pthread barrier
  int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count) {
    DEBUG("Call replaced pthread_barrier_init");
    return Qthread::GetInstance().BarrierInit(barrier, attr, count);
  }

  int pthread_barrier_wait(pthread_barrier_t *barrier) {
    DEBUG("Call replaced pthread_barrier_wait");
    return Qthread::GetInstance().BarrierWait(barrier);
  }

  int pthread_barrier_destroy(pthread_barrier_t *barrier) {
    DEBUG("Call replaced pthread_barrier_destroy");
    return Qthread::GetInstance().BarrierDestroy(barrier);
  }

}



