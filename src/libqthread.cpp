#include "qthread.h"
#include "debug.h"



// The instance of Qthread is constructed before main
Qthread Qthread::instance;



/**
 * Shims to call replaced pthread subroutines
 */
extern "C" {
  /////////////////////////////////////////////////////////////////////////////// Qthread Defined
  // This API let thread temporarily leave the token passing game at any given time
  // For example, if a thread doesn't acquire locks for a long time, it can call


  void qthread_bla(void) {
    if (qthread_initialized) {
      Qthread::GetInstance().Bla();
    }
    return;
  }

  void qthread_blabla(void) {
    if (qthread_initialized) {
      Qthread::GetInstance().Blabla();
    }
    return;
  }


  int qthread_hibernate_thread(size_t tid) {
    if (qthread_initialized) {
      return Qthread::GetInstance().HibernateThread(tid);
    }
    return 0;
  }

  int qthread_wakeup_thread(size_t tid) {
    if (qthread_initialized) {
      return Qthread::GetInstance().WakeUpThread(tid);
    }
    return 0;
  }

  int qthread_hibernate_thread_by_pid(pthread_t pid) {
    if (qthread_initialized) {
      return Qthread::GetInstance().HibernateThread(pid);
    }
    return 0;
  }

  int qthread_wakeup_thread_by_pid(size_t pid) {
    if (qthread_initialized) {
      return Qthread::GetInstance().WakeUpThread(pid);
    }
    return 0;
  }

  void qthread_dummy_sync(void) {
    if (qthread_initialized) {
      Qthread::GetInstance().DummySync();
    }
    return;    
  }


  // For the convenience of tesing. 
  size_t qthread_my_tid(void) {
    return my_tid;
  }

  /////////////////////////////////////////////////////////////////////////////// Pthread Defined
  /////////////////////////////////////////////////////////////////////////////// Pthread basic
  
  int pthread_create(pthread_t * tid, const pthread_attr_t * attr, ThreadFunction func, void * arg) {
    if (qthread_initialized) {
      return Qthread::GetInstance().Spawn(tid, attr, func, arg);
    }
    return 0;
  }

  int pthread_cancel(pthread_t tid) {
    if (qthread_initialized) {
      return ppthread_cancel(tid);
    }
    return 0;
  }

  int pthread_join(pthread_t tid, void ** val) {
    if (qthread_initialized) {
      return Qthread::GetInstance().Join(tid, val);
    }
    return 0;
  }

  int pthread_exit(void * value_ptr) {
    if (qthread_initialized) {
      return Qthread::GetInstance().Exit(value_ptr);
    }
    return 0;
  }

  /////////////////////////////////////////////////////////////////////////////// Pthread Mutex

  int pthread_mutex_init(pthread_mutex_t * mutex, const pthread_mutexattr_t  *attr) {
    if (qthread_initialized) {
      return Qthread::GetInstance().MutexInit(mutex, attr);
    }
    return 0;
  }


  // Provide a novel api to support "wait" version mutex lock
  // int pthread_mutex_waitlock(pthread_mutex_t * mutex) {
  //   if (qthread_initialized) {
  //     return Qthread::GetInstance().MutexWaitLock(mutex);
  //   }
  //   return 0;
  // }  

  int pthread_mutex_lock(pthread_mutex_t * mutex) {
    if (qthread_initialized) {
      return Qthread::GetInstance().LockAcquire(mutex);
      // return Qthread::GetInstance().MutexLock(mutex);

    }
    return 0;
  }
  
  int pthread_mutex_unlock(pthread_mutex_t * mutex) {
    if (qthread_initialized) {
      return Qthread::GetInstance().LockRelease(mutex);
      // return Qthread::GetInstance().MutexUnlock(mutex);

    }
    return 0;
  }

  int pthread_mutex_trylock(pthread_mutex_t * mutex) {
    if (qthread_initialized) {
      return Qthread::GetInstance().MutexTrylock(mutex);
    }
    return 0;
  }

  int pthread_mutex_destory(pthread_mutex_t * mutex) {
    if (qthread_initialized) {
      return Qthread::GetInstance().MutexDestroy(mutex);
    }
    return 0;
  }

  // pthread spinlock
  int pthread_spin_init(pthread_spinlock_t * spinner, int shared) {
    if (qthread_initialized) {
      return Qthread::GetInstance().SpinInit(spinner, shared);
    }
    return 0;
  }

  int pthread_spin_lock(pthread_spinlock_t * spinner) {
    if (qthread_initialized) {
      return Qthread::GetInstance().SpinLock(spinner);
    }
    return 0;
  }

  int pthread_spin_unlock(pthread_spinlock_t * spinner) {
    if (qthread_initialized) {
      return Qthread::GetInstance().SpinUnlock(spinner);
    }
    return 0;
  }

  int pthread_spin_trylock(pthread_spinlock_t * spinner) {
    if (qthread_initialized) {
      return Qthread::GetInstance().SpinTrylock(spinner);
    }
    return 0;
  }

  int pthread_spin_destroy(pthread_spinlock_t * spinner) {
    if (qthread_initialized) {
      return Qthread::GetInstance().SpinDestroy(spinner);
    }
    return 0;
  }

  /////////////////////////////////////////////////////////////////////////////// Pthread Read/Write Lock

  int pthread_rwlock_init(pthread_rwlock_t * rwlock, const pthread_rwlockattr_t * attr) {
    if (qthread_initialized) {
      return Qthread::GetInstance().RwLockInit(rwlock, attr);
    }
    return 0;
  }

  int pthread_rwlock_rdlock(pthread_rwlock_t * rwlock) {
    if (qthread_initialized) {
      return Qthread::GetInstance().RdLock(rwlock);
    }
    return 0;
  }

  int pthread_rwlock_wrlock(pthread_rwlock_t * rwlock) {
    if (qthread_initialized) {
      return Qthread::GetInstance().WrLock(rwlock);
    }
    return 0;
  }

  int pthread_rwlock_tryrdlock(pthread_rwlock_t * rwlock) {
    if (qthread_initialized) {
      return Qthread::GetInstance().RdTryLock(rwlock);
    }
    return 0;
  }

  int pthread_rwlock_trywrlock(pthread_rwlock_t * rwlock) {
    if (qthread_initialized) {
      return Qthread::GetInstance().WrTryLock(rwlock);
    }
    return 0;
  }

  int pthread_rwlock_unlock(pthread_rwlock_t * rwlock) {
    if (qthread_initialized) {
      return Qthread::GetInstance().RwUnlock(rwlock);
    }
    return 0;
  }

  int pthread_rwlock_destroy(pthread_rwlock_t * rwlock) {
    if (qthread_initialized) {
      return Qthread::GetInstance().RwLockDestroy(rwlock);
    }
    return 0;
  }

  /////////////////////////////////////////////////////////////////////////////// Pthread Condition Variable

  int pthread_cond_init(pthread_cond_t * cond, const pthread_condattr_t * attr) {
    if (qthread_initialized) {
      return Qthread::GetInstance().CondInit(cond, attr);
    }
    return 0;
  }

  int pthread_cond_wait(pthread_cond_t * cond, pthread_mutex_t * mutex) {
    if (qthread_initialized) {
      return Qthread::GetInstance().CondWait(cond, mutex);
    }
    return 0;
  }

  int pthread_cond_signal(pthread_cond_t * cond) {
    if (qthread_initialized) {
      return Qthread::GetInstance().CondSignal(cond);
    }
    return 0;
  }

  // Broadcast has not been implemented
  int pthread_cond_broadcast(pthread_cond_t * cond) {
    // if (qthread_initialized) {
    //   return Qthread::GetInstance().CondBroadcast(cond);
    // }
    assert(0);
    return 0;
  }

  int pthread_cond_destroy(pthread_cond_t * cond) {
    if (qthread_initialized) {
      return Qthread::GetInstance().CondDestroy(cond);
    }
    return 0;
  }

  // pthread barrier
  int pthread_barrier_init(pthread_barrier_t * barrier, const pthread_barrierattr_t * attr, unsigned int count) {
    if (qthread_initialized) {
      return Qthread::GetInstance().BarrierInit(barrier, attr, count);
    }
    return 0;
  }

  // Barrier_wait has not been implemented
  int pthread_barrier_wait(pthread_barrier_t * barrier) {
    assert(0);
    // if (qthread_initialized) {    
    //   return Qthread::GetInstance().BarrierWait(barrier);
    // }
    return 0;
  }

  int pthread_barrier_destroy(pthread_barrier_t * barrier) {
    if (qthread_initialized) {
      return Qthread::GetInstance().BarrierDestroy(barrier);
    }
    return 0;
  }

}






