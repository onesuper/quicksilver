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
  // For example, if the main process doesn't acquire locks at all, it can call it
  // and let others go on to play this game
  int qthread_leave_game(void) {
    if (qthread_initialized) {
      return Qthread::GetInstance().DeregisterMe();
    }
    return 0;
  }

  // We can call this API to wake the thread up to play game
  // It will return false if the thread is already in the game
  int qthread_join_game(void) {
    if (qthread_initialized) {
      return Qthread::GetInstance().RegisterThread(my_tid);
    }
    return 0;
  }


  // For the convenience of tesing. 
  size_t qthread_my_tid(void) {
    return my_tid;
  }

  /////////////////////////////////////////////////////////////////////////////// Pthread Defined
  // pthread basic
  int pthread_create(pthread_t * tid, const pthread_attr_t * attr, ThreadFunction func, void * arg) {
    if (qthread_initialized) {
      return Qthread::GetInstance().Create(tid, attr, func, arg);
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
      return ppthread_join(tid, val);
    }
    return 0;
  }

  int pthread_exit(void * value_ptr) {
    if (qthread_initialized) {
      Qthread::GetInstance().DeregisterMe();
      return ppthread_exit(value_ptr);
    }
    return 0;
  }

  // pthread mutex
  int pthread_mutex_init(pthread_mutex_t * mutex, const pthread_mutexattr_t  *attr) {
    if (qthread_initialized) {
      return Qthread::GetInstance().MutexInit(mutex, attr);
    }
    return 0;
  }

  int pthread_mutex_lock(pthread_mutex_t * mutex) {
    if (qthread_initialized) {
      return Qthread::GetInstance().MutexLock(mutex);
    }
    return 0;
  }
  
  // Provide a novel api to support "wait" version mutex lock
  int pthread_mutex_waitlock(pthread_mutex_t * mutex) {
    if (qthread_initialized) {
      return Qthread::GetInstance().MutexWaitLock(mutex);
    }
    return 0;
  }  

  int pthread_mutex_unlock(pthread_mutex_t * mutex) {
    if (qthread_initialized) {
      return Qthread::GetInstance().MutexUnlock(mutex);
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

  // pthread rwlock
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

  // pthread condition variables
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

  int pthread_cond_broadcast(pthread_cond_t * cond) {
    if (qthread_initialized) {
      return Qthread::GetInstance().CondBroadcast(cond);
    }
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

  int pthread_barrier_wait(pthread_barrier_t * barrier) {
    if (qthread_initialized) {    
      return Qthread::GetInstance().BarrierWait(barrier);
    }
    return 0;
  }

  int pthread_barrier_destroy(pthread_barrier_t * barrier) {
    if (qthread_initialized) {
      return Qthread::GetInstance().BarrierDestroy(barrier);
    }
    return 0;
  }

}


/**
 * Fake entry point of thread. We use it to unregister thread inside the thread body
 */
void * fake_thread_entry(void * param) {

  ThreadParam * obj = static_cast<ThreadParam *>(param);
  
  // Dump parameters
  ThreadFunction my_func = obj->func;
  void * my_arg = (void *) obj->arg;
  my_tid = obj->index;

  // Unlock after copying out paramemters
  ppthread_mutex_unlock(&spawn_lock);


  //// Register when the thread is spawned
  //Qthread::GetInstance().RegisterMe();

  // Call the real thread function
  void * retval = my_func(my_arg);

  // Let each thread deregister it self
  Qthread::GetInstance().DeregisterMe();

  return retval;
}





