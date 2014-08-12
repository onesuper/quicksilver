#ifndef _QTHREAD_H_
#define _QTHREAD_H_

#include <vector>
#include <cerrno>
#include <cstddef>
#include <sched.h>
#include "ppthread.h" 
#include "debug.h"
#include "timer.h"
#include "random.h"



#define INVALID_TID 4294967296

// Thread local storage: use to let thread know who he is
__thread size_t my_tid;


class Qthread {

private:

  static Qthread _instance;

  // Used to manage threads
  class ThreadEntry {
  public:
    size_t tid;
    ThreadEntry() {}
    ThreadEntry(size_t _tid): tid(_tid) {}
  };

  // Maintain all the active thread entries in a vector 
  std::vector<ThreadEntry> _active_entries;

  // Pointing to the thread entry who holds the token
  // Could be negative (-1 = no one has token)
  volatile int _token_pos;

  // The thread id of the token owner
  volatile size_t _token_tid;

  // All threads use this lock to prevent from data race
  pthread_mutex_t _mutex;

  // Timing info
  Timer _stat_total;
  Timer _stat_serial;
  

public:
  Qthread():
  _token_pos(-1),
  _token_tid(INVALID_TID),
  _active_entries(),
  _stat_total(),
  _stat_serial() 
  {

    // Initializing the references to pthread subroutines
    DEBUG("Registering pthread instance");
    init_pthread_reference();

    // set up the random number generator
    settable(12345,65435,34221,12345,9983651,95746118);

    ppthread_mutex_init(&_mutex, NULL);
    _stat_total.Start();

  }

  ~Qthread() {
    _stat_total.Pause();
    ppthread_mutex_destroy(&_mutex);

    printf("Total Time: %ld\n", _stat_total.Total());
    printf("Serial Time: %ld @ %ld\n", _stat_serial.Total(), _stat_serial.Times()); 
  }

  static Qthread& GetInstance(void) {
    return _instance;
  }

  void RegisterThread(size_t index) {

    lock();
    DEBUG("Register: %lu", index);

    // Create a new entry according to tid
    ThreadEntry entry(index);

    // Link the newly created threadEntry into the active thread list
    _active_entries.push_back(entry);

    // Initially, token belongs to the first created thread
    if (_token_pos == -1) {
      _token_pos = 0;
      _token_tid = _active_entries[0].tid;
    }

#ifdef DEBUG
    printf("Entries: ");
    for (int i=0; i<_active_entries.size(); i++) {
      printf("%lu,", _active_entries[i].tid);
    }
    printf("\n");
    printf("Token: %lu @ [%d]\n", ((_token_pos >= 0 ) ? _token_tid : INVALID_TID), _token_pos);
#endif

    unlock();
    return;
  }

  // We do not treat thread deregisteration as a sync point
  void DeregisterMe(void) {


    lock();
    DEBUG("Deregister: %lu", my_tid);

    // Locate the target thread in the entires
    std::vector<ThreadEntry>::size_type pos;
    for (pos=0; pos<_active_entries.size(); pos++) {
      if (_active_entries[pos].tid == my_tid) {
        break;   
      }
    }
   
    // Delete the entry from the active list. 
    // The next entry gets the token for free (_token_pos doesn't move)
    _active_entries.erase(_active_entries.begin() + pos);

    // Decide the token pos

    // Get back the token if there's no active threads
    if (_active_entries.empty()) { 
      _token_pos = -1;
      _token_tid = INVALID_TID;
      DEBUG("CallbackToken: %lu", my_tid);
      unlock();
      return;
    } 
              
    // If the pos to delete is above the token pos,
    // deleting it will affect the token ownership
    // Otherwise, deleting entires[pos] will not affect the token ownership 
    if (pos < _token_pos) {
      _token_pos--;
    } 
    
    // loop back to the first position
    if (pos == _active_entries.size()) { 
      _token_pos = 0;
    }

    // Set token_tid according to the latest token_pos
    _token_tid = _active_entries[_token_pos].tid;

#ifdef DEBUG
    printf("Entries: ");
    for (int i=0; i<_active_entries.size(); i++) {
      printf("%lu,", _active_entries[i].tid);
    }
    printf("\n");
    printf("Token: %lu @ [%d]\n", ((_token_pos >= 0 ) ? _token_tid : INVALID_TID), _token_pos);
#endif

    unlock();
    return;
  }


  int MutexInit(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    return ppthread_mutex_init(mutex, attr);
  }

  int MutexLock(pthread_mutex_t *mutex) {
    DEBUG("MutexLock: %lu", my_tid);
    _stat_serial.Start();
    int retval = -1;
    while (true) { // As long as retval==EBUSY, the loop will go on
      waitForToken();
      retval = ppthread_mutex_trylock(mutex);
      // Any thread having token has the right to acquire the lock
      if (retval == EBUSY) {
        // If thread fails to acquire the lock, then pass the token immediately
        // This prevent the case that thread sleep on the mutex while holding token
        DEBUG("MutexLockBusy: %lu", my_tid);
        passToken();
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("MutexLockAcquired: %lu", my_tid);
        passToken();
        break;
      }
    }
    _stat_serial.Pause();        
    return retval;
  }


  int MutexTrylock(pthread_mutex_t *mutex) {
    DEBUG("MutexTryLock: %lu", my_tid);
    _stat_serial.Start();
    int retval = -1;
    while (true) {
      waitForToken();          
      // Acqurie the lock anyway, let other competitors sleep
      retval = ppthread_mutex_trylock(mutex);
      // If trylock failed, return directly. No need to give yield the lock 
      if (retval == EBUSY) {
        DEBUG("MutexTryLockBusy: %lu)", my_tid);
        passToken();
        return EBUSY;  // This is a trylock, so we never wait until acquire lock
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("MutexTryLockFailed: %lu", my_tid);
        passToken();
        break;
      }
    }    
    _stat_serial.Pause();        
    return retval;
  }

  // Provide a wait lock version for pthread mutex lock
  // In this version, each thread acquires the lock anyway before
  // checking the token ownership. If it doesn't own token just yield it 
  // and repeat to acuquire the lock and then check the token...
  int MutexWaitLock(pthread_mutex_t *mutex) {
    DEBUG("MutexWaitLock: %lu", my_tid);
    _stat_serial.Start();
    int retval = -1;
    while (true) {          
      // Acqurie the lock anyway, let other competitors sleep
      retval = ppthread_mutex_lock(mutex);
      DEBUG("MutexLockAcquire: %lu", my_tid);
      // Check whether I have the token
      if (my_tid != _token_tid) {
        // if I have no token, yield the lock
        DEBUG("MutexLockYield: %lu", my_tid);
        ppthread_mutex_unlock(mutex); 
      } else {
        // Lock acquired here. Pass token and continues its execution
        passToken();
        break;
      }
    }
    _stat_serial.Pause();        
    return retval;
  }

  int MutexUnlock(pthread_mutex_t *mutex) {
    int retval = -1;
    DEBUG("MutexUnlock: %lu", my_tid);
    waitForToken();
    retval = ppthread_mutex_unlock(mutex);
    DEBUG("MutexLockReleased: %lu", my_tid);
    passToken();
    return retval;
  }

  int MutexDestroy(pthread_mutex_t *mutex) {
    return ppthread_mutex_destroy(mutex);
  }

  // spin
  int SpinInit(pthread_spinlock_t *spinner, int shared) {
    return ppthread_spin_init(spinner, shared);
  }

  int SpinLock(pthread_spinlock_t *spinner) {
    DEBUG("SpinLock: %lu", my_tid);
    _stat_serial.Start();
    int retval = -1;
    while (true) {
      waitForToken();
      retval = ppthread_spin_trylock(spinner);
      // Any thread having token has the right to acquire the lock
      if (retval == EBUSY) {
        // If thread fails to acquire the lock, then pass the token immediately
        DEBUG("SpinLockBusy: %lu", my_tid);
        passToken();
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("SpinLockAcquire: %lu", my_tid);
        passToken();
        break;
      }
    }
    _stat_serial.Pause();
    return retval;
  }

  int SpinTrylock(pthread_spinlock_t *spinner) {
    DEBUG("SpinTryLock: %lu", my_tid);
    _stat_serial.Start();
    int retval = -1;
    while (true) {
      waitForToken();
      retval = ppthread_spin_trylock(spinner);
      // if trylock failed, we no longer check token, since we do not lock 
      if (retval == EBUSY) {
        DEBUG("SpinTryLockBusy: %lu", my_tid);
        passToken();
        return EBUSY;
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("SpinTryLockAcquired: %lu", my_tid);
        passToken();
        break;
      }      
    }
    _stat_serial.Pause();        
    return retval;
  }

  int SpinUnlock(pthread_spinlock_t *spinner) {
    int retval = -1;
    DEBUG("SpinUnlock: %lu", my_tid);
    waitForToken();
    retval = ppthread_spin_unlock(spinner);
    DEBUG("SpinLockReleased: %lu", my_tid);
    passToken();
    return retval;
  }

  int SpinDestroy(pthread_spinlock_t *spinner) {
    return ppthread_spin_destroy(spinner);
  }


  // Cond
  int CondInit(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    return 0;
  }

  int CondWait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    return 0;
  }

  int CondSignal(pthread_cond_t *cond) {
    return 0;
  }

  int CondBroadcast(pthread_cond_t *cond) {
    return 0;
  }

  int CondDestroy(pthread_cond_t *cond) {
    return 0;
  }

  int BarrierInit(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count) {
    return 0;
  }

  int BarrierWait(pthread_barrier_t *barrier) {
    return 0;
  }

  int BarrierDestroy(pthread_barrier_t *barrier) {
    return 0;
  }


private:

  // Spin until I get the token
  void waitForToken(void) {
    DEBUG("WaitToken: %lu", my_tid);
    while (my_tid != _token_tid) { 
      __asm__ __volatile__ ("mfence");
    }
    DEBUG("GetToken: %lu", my_tid);
  }

  // Force thread to pass his token to the next thread in the active list
  // There is no need to acquire lock here since there's only one thread can call passToken
  void passToken(void) {

    // Make sure only the token owner can pass token
    if (my_tid != _token_tid) {
      DEBUG("Error! <%lu> attempts to pass token but the token belongs to %lu", my_tid, _token_tid);
      assert(0);
      return;
    }

#ifdef RANDOM_NEXT
    _token_pos = (_token_pos + 1 + KISS % _active_entries.size()) % _active_entries.size();
#else
    _token_pos = (_token_pos + 1) % _active_entries.size();
#endif
    
    // Besides passToken, _token_pos is also changed when deregister thread
    lock();

    // Setup token id according to token position
    _token_tid = _active_entries[_token_pos].tid;
    DEBUG("PassToken: %lu", my_tid);

    unlock();
    return;
  }

  inline void lock(void) {
    ppthread_mutex_lock(&_mutex);
    return;
  }

  inline void unlock(void) {
    ppthread_mutex_unlock(&_mutex);
    return;
  }


};


#endif
