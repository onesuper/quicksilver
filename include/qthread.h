#ifndef _QTHREAD_H_
#define _QTHREAD_H_

#include <vector>
#include <cerrno>
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

    DEBUG("Total Time: %ld\n", _stat_total.Total());
    DEBUG("Serial Time: %ld @ %ld\n", _stat_serial.Total(), _stat_serial.Times()); 
  }

  static Qthread& GetInstance(void) const {
    return &_instance;
  }

  void RegisterThread(size_t tid) {

    lock();
    DEBUG("Registering thread %lu ...", tid);

    // Create a new entry according to tid
    ThreadEntry entry(tid);

    // Link the newly created threadEntry into the active thread list
    _active_entries.push_back(entry);

    // Initially, token belongs to the first created thread
    if (_token_pos == -1) {
      _token_pos = 0;
      _token_id = _active_entries[0].tid;
    }

#ifdef DEBUG
    DEBUG("Printing the active entries...")
    for (int i=0; i<_active_entries.size(); i++) {
      DEBUG("%lu", _active_entries[i].tid);
    }
    DEBUG("Token: %lu @ [%d]", ((_token_pos >= 0 ) ? _token_tid : INVALID_TID), _token_pos);
#endif

    unlock();
    return;
  }


  void DeregisterThread(void) {

    lock();
    DEBUG("Deregistering thread %lu ...", my_tid);

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
      DEBUG("Since there is no thread left, token is called back");
      unlock();
      return;
    } 
              
    // loop back to the first position
    if (pos == _active_entries.size()) { 
      _token_pos = 0;
    }

    // Set token_tid according to the latest token_pos
    _token_tid = _active_entries[_token_pos];

#ifdef DEBUG
    DEBUG("Printing the active entries...")
    for (int i=0; i<_active_entries.size(); i++) {
      DEBUG("%lu", _active_entries[i].tid);
    }
    DEBUG("Token: %lu @ [%d]", ((_token_pos >= 0 ) ? _token_tid : INVALID_TID), _token_pos);
#endif

    DEBUG("%lu is deregisterred", index);
    unlock();
    return;
  }


  int MutexInit(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    return ppthread_mutex_init(mutex, attr);
  }




  int MutexLock(pthread_mutex_t *mutex) {
    DEBUG("Thread %lu calls mutex_lock", my_tid);
    _stat_serial.Start();
    int retval = -1;

    while (true) {

      waitForToken();

      // Any thread having token has the right to acquire the lock
      if (EBUSY == ppthread_mutex_trylock(mutex)) {
        // If thread fails to acquire the lock, then pass the token immediately
        // This prevent the case that thread sleep on the mutex while holding token
        passToken();
      } else {
        // If we arrive here, this thread must have acquired the lock
      }
    }

    passToken();
    _stat_serial.Pause();        
    return retval;
  }


  // Provide a wait version for mutex lock
  // In this version, each thread acquires the lock before checking the token ownership
  int MutexWaitLock(pthread_mutex_t *mutex) {
    DEBUG("Thread %lu calls mutex_wait_lock", my_tid);

    _stat_serial.Start();
    int retval = -1;

    while (true) {          
      // Acqurie the lock anyway, let other competitors sleep
      retval = ppthread_mutex_lock(mutex);
      DEBUG("Thread %lu acquires the lock anyway", my_tid);
      
      // Check whether I have the token
      if (my_tid != _token_tid) {
        // if I have no token, yield the lock
        DEBUG("Thread %lu yields the lock since he has no token (%lu)", my_tid, _token_tid);
        ppthread_mutex_unlock(mutex); 
      } else {
        break;
      }
    }

    // Lock acquired here. Pass token and continues its execution
    passToken();
    _stat_serial.Pause();        
    return retval;
  }

  int MutexUnlock(pthread_mutex_t *mutex) {
    int retval = -1;
    DEBUG("Thread %lu attempts to call mutex unlock", my_tid);
    waitForToken();
    retval = ppthread_mutex_unlock(mutex);
    passToken();
    return retval;
  }

  int MutexTrylock(pthread_mutex_t *mutex) {
    DEBUG("Thread %lu calls mutex trylock", my_tid);
    _stat_serial.Start();
    int retval = -1;

    while (true) {          
      // Acqurie the lock anyway, let other competitors sleep
      retval = ppthread_mutex_trylock(mutex);
      DEBUG("Thread %lu tires to grab the mutex lock", my_tid);
      
      // If trylock failed, return directly. No need to give yield the lock 
      if (retval == EBUSY) {
        DEBUG("Thread %lu fails to grab the token (EBUSY)", my_tid);
        return EBUSY;
      }

      // if I have no token, yield the lock
      if (my_tid != _token_tid) {
        DEBUG("Thread %lu yields the lock since he has no token (%lu)", my_tid, _token_tid);
        ppthread_mutex_unlock(mutex); 
      } else {
        break;
      }
    }    

    _stat_serial.Pause();        
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
    DEBUG("<%lu> call spin lock", my_tid);
    _stat_serial.Start();
    int retval = -1;

    waitForToken();
    retval = ppthread_spin_lock(spinner);
    DEBUG("Thread %lu acquires the spin lock", my_tid);

    passToken();
    _stat_serial.Pause();
    return retval;
  }

  int SpinUnlock(pthread_spinlock_t *spinner) {
    int retval = -1;
    DEBUG("Thread %lu attempts to call spin unlock", my_tid);
    waitForToken();
    retval = ppthread_spin_unlock(spinner);
    passToken();
    return retval;
  }

  int SpinTrylock(pthread_spinlock_t *spinner) {
    DEBUG("<%lu> call spin trylock", my_tid);
    _stat_serial.Start();
    int retval = -1;

    while (true) {
      // Acqurie the lock anyway, let other competitors busy-wait
      retval = ppthread_spin_trylock(spinner);
      DEBUG("Thread %lu try to grab the spin lock", my_tid);

      // if trylock failed, we no longer check token, since we do not lock 
      if (retval == EBUSY) {
        DEBUG("Thread %lu fail to grab the token (EBUSY)", my_tid);
        return EBUSY;
      }

      if (_my_tid != _token_id) {
        DEBUG("Thread %lu yields the lock since he has no token (%lu)", my_tid, _token_tid);
        ppthread_spin_unlock(spinner); // give up the lock
      } else {
        break;
      }
    }

    _stat_serial.Pause();        
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
    DEBUG("Thread %lu starts busy waiting for token", my_tid);
    while (my_tid != _token_tid) { 
      __asm__ __volatile__ ("mfence");
    }
  }

  // Force thread to pass his token to the next thread in the active list
  // There is no need to acquire lock here since there's only one thread can call passToken
  void passToken(void) {
/*
    if (my_tid != _token_tid) {
      DEBUG("Error! <%lu> attempts to pass token but the token belongs to %lu", my_tid, _token_tid);
      assert(0);
      return;
    }
*/
#ifdef RANDOM_NEXT
    _token_pos = (_token_pos + 1 + KISS % _active_entries.size()) % _active_entries.size();
#else
    _token_pos = (_token_pos + 1) % _active_entries.size();
#endif

    // Setup token id according to token position
    _token_tid = _active_entries[_token_pos].tid;

    DEBUG("<%lu> Token is now passed to %lu", my_tid, _token_tid);
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