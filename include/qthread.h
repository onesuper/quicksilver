#ifndef _QTHREAD_H_
#define _QTHREAD_H_

#include <vector>
#include <cerrno>
#include <cstddef>
#include "ppthread.h" 
#include "debug.h"
#include "timer.h"
#include "random.h"



//#define RANDOM_NEXT
#define INVALID_TID 4294967296

// How soon can a lock be owned?
#define LOCK_BUDGET 3

// Thread local storage: let each thread know who he is
__thread size_t my_tid;


class Qthread {

private:

  static Qthread _instance;

  ///////////////////////////////////////////////////////////////////// ThreadEntry  
  class ThreadEntry {
  public:
    size_t tid;
    volatile bool owning_lock; // if this thread is owning a lock
    ThreadEntry() {}
    ThreadEntry(size_t _tid): tid(_tid), owning_lock(false) {}
  };

  ///////////////////////////////////////////////////////////////////// Lockownership  
  class LockOwnership {
  public:
    volatile bool is_used;
    volatile size_t owner_thread;
    volatile unsigned int budget;
    volatile void * owned_lock; // The address of the lock we are tracking

    LockOwnership(): 
      is_used(false),
      owner_thread(),
      budget(0),
      owned_lock(NULL)
    {}

    // Take the ownership of a certain lock. 
    inline void Take(void * lock) {
      is_used = true;      // Make the ownership Exclusive
      owner_thread = my_tid;
      owned_lock = lock;
      budget = LOCK_BUDGET; // Recharge the budget
      return;
    }

    inline void Yield(void) {
      is_used = false;
      owner_thread = INVALID_TID;
      owned_lock = NULL;
      budget = 0;
    }


  };

  // TODO: Only one lock is tracked currently, try to track more locks in next version.
  // At any given time, only one lock can be owned by one thread in the system 
  // And during its ownership (is_used==true), other threads must not owned any locks
  LockOwnership _lock_ownership;

  // Maintain all the active thread entries in a vector 
  std::vector<ThreadEntry> _active_entries;

  // Pointing to the thread entry who holds the token
  // Could be negative (-1 = no one has token)
  volatile int _token_pos;

  // The thread id of the token owner
  volatile size_t _token_tid;

  // All threads use this lock to prevent from data race
  pthread_mutex_t _mutex;

  // Wall-clock (total elapsed time)
  Timer _stat_total;
  // Time consumed in waiting token
  Timer _stat_serial;   
  

public:
  ///////////////////////////////////////////////////////////////////// Constructor  
  Qthread():
  _lock_ownership(),
  _active_entries(),
  _token_pos(-1),
  _token_tid(INVALID_TID),
  _stat_total(),
  _stat_serial() 
  {

    // Initializing the references to pthread subroutines
    DEBUG("Registering pthread instance\n");
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

  ///////////////////////////////////////////////////////////////////// Registeration
  void RegisterThread(size_t index) {
    // FIXME: if we treat thread creation as a sync point, then this lock is unnecessary 
    lock_();
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

#if 0
    printf("Entries: ");
    for (int i=0; i<_active_entries.size(); i++) {
      printf("%lu,", _active_entries[i].tid);
    }
    printf("\n");
    printf("Token: %lu @ [%d]\n", ((_token_pos >= 0 ) ? _token_tid : INVALID_TID), _token_pos);
#endif

    unlock_();
    return;
  }


  // We do not treat thread deregisteration as a sync point
  // So it must be protected by lock
  void DeregisterThread(size_t tid) {

    lock_();
    DEBUG("Deregister: %lu", tid);

    // First of all, locate the target thread in the entires
    unsigned int pos;
    for (pos=0; pos<_active_entries.size(); pos++) {
      if (_active_entries[pos].tid == tid) {
        break;   
      }
    }
   
    // Before we delete the thread entry, check whether he has owned any lock
    // This case happens when the lock's budget has not been costed but he stops
    // acquiring that lock. This is really bad, because others may also want to acquire that lock.
    // If so, force it to yield ownership before quiting.
    // TODO: Checking the 'dead' ownership when deregistering the thread is a
    // little bit late. Is there any possible to spot dead ownership as early as possible? 
    if (_lock_ownership.is_used == true && _active_entries[pos].owning_lock == true) {
      _lock_ownership.Yield();
    }

    // Delete the entry from the active list. 
    // The next entry gets the token for free (_token_pos doesn't move)
    _active_entries.erase(_active_entries.begin() + pos);

    // Decide the token pos
    // 1. Get back the token if there's no active threads
    if (_active_entries.empty()) { 
      _token_pos = -1;
      _token_tid = INVALID_TID;
      DEBUG("CallbackToken: %lu", tid);
      unlock_();
      return;
    }            
    // 2. If the pos to delete is above the token pos,
    // deleting it will affect the token ownership
    // Otherwise, deleting entires[pos] will not affect the token ownership 
    if (pos < (unsigned int) _token_pos) {
      _token_pos--;
    } 
    // 3. loop back to the first position
    if (pos == _active_entries.size()) { 
      _token_pos = 0;
    }
    // 4. Set token_tid according to the latest token_pos
    _token_tid = _active_entries[_token_pos].tid;

#if 0
    printf("Entries: ");
    for (int i=0; i<_active_entries.size(); i++) {
      printf("%lu,", _active_entries[i].tid);
    }
    printf("\n");
    printf("Token: %lu @ [%d]\n", ((_token_pos >= 0 ) ? _token_tid : INVALID_TID), _token_pos);
#endif

    unlock_();
    return;
  }


  //////////////////////////////////////////////////////////////////////////// Mutex
  // FIXME: If we do not wrap the mutex structure, this wrapper function can be removed
  // So as to MutexDestory(), SpinlockInit(), SpinlockDestroy()...
  int MutexInit(pthread_mutex_t * mutex, const pthread_mutexattr_t * attr) {
    return ppthread_mutex_init(mutex, attr);
  }

  int MutexLock(pthread_mutex_t * mutex) {
    int retval = -1;
    DEBUG("MutexLock: %lu", my_tid);
    _stat_serial.Start();

    // Acuqire lock as its onwer, so no bother to wait token to progress
    if (_lock_ownership.is_used == true && _lock_ownership.owner_thread == my_tid) {
      _lock_ownership.budget--; // cost budgets
      DEBUG("MutexLockAcqOwned(%p): %lu", mutex, my_tid);
      return ppthread_mutex_lock(mutex);
    } 

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
        DEBUG("MutexLockAcq(%p): %lu", mutex, my_tid); // acquire
        // Claim ownership
        // We only take the ownership of a lock in the serial phase to ensure determinism
        if (_lock_ownership.is_used == false) {  
          _lock_ownership.Take((void *) mutex);
          _lock_ownership.budget--; // cost some budgets
          DEBUG("TakeOwnership(%p): %lu", mutex, my_tid);
          // Register the corresponding info in the entry, then we will never pass token to him
          lock_();
          ThreadEntry * entry = getThreadEntry(my_tid);
          entry->owning_lock = true;
          unlock_();
        }
        passToken();
        break;
      }
    }
    _stat_serial.Pause();        
    return retval;
  }


  int MutexTrylock(pthread_mutex_t * mutex) {
    int retval = -1;
    DEBUG("MutexTryLock: %lu", my_tid);
    _stat_serial.Start();
    while (true) {
      waitForToken();          
      // Acqurie the lock anyway, let other competitors sleep
      retval = ppthread_mutex_trylock(mutex);

      // This is a trylock, so we do not desire for locks
      // If trylock failed, just return directly
      if (retval == EBUSY) {
        DEBUG("MutexTryLockBusy: %lu)", my_tid);
        passToken();
        return EBUSY;  
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("MutexTryLockAcq: %lu", my_tid);
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
  int MutexWaitLock(pthread_mutex_t * mutex) {
    int retval = -1;
    DEBUG("MutexWaitLock: %lu", my_tid);
    _stat_serial.Start();
    while (true) {          
      // Acqurie the lock anyway, let other competitors sleep
      retval = ppthread_mutex_lock(mutex);
      DEBUG("MutexLockAcq: %lu", my_tid);
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

  int MutexUnlock(pthread_mutex_t * mutex) {
    int retval = -1;
    DEBUG("MutexUnlock: %lu", my_tid);

    // Release lock as onwer, no need to wait token to progress
    if (_lock_ownership.is_used == true && _lock_ownership.owner_thread == my_tid) {
      DEBUG("MutexLockRelOwned(%p): %lu", mutex, my_tid);
      // Running out of budget, thread yields its ownership
      if (_lock_ownership.budget == 0) {
        DEBUG("YieldOnwership(%p): %lu", mutex, my_tid);
        // Deregister the corresponding info in the entry
        _lock_ownership.Yield();
        lock_();
        ThreadEntry * entry = getThreadEntry(my_tid);
        entry->owning_lock = false;
        unlock_();
      }
      return ppthread_mutex_unlock(mutex);
    } 

    waitForToken();
    retval = ppthread_mutex_unlock(mutex);
    DEBUG("MutexLockRel(%p): %lu", mutex, my_tid);
    passToken();
    return retval;
  }

  int MutexDestroy(pthread_mutex_t * mutex) {
    return ppthread_mutex_destroy(mutex);
  }

  ///////////////////////////////////////////////////////////////////// Spinlock
  int SpinInit(pthread_spinlock_t * spinner, int shared) {
    return ppthread_spin_init(spinner, shared);
  }

  int SpinLock(pthread_spinlock_t * spinner) {
    int retval = -1;
    DEBUG("SpinLock: %lu", my_tid);
    _stat_serial.Start();
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
        DEBUG("SpinLockAcq: %lu", my_tid);
        passToken();
        break;
      }
    }
    _stat_serial.Pause();
    return retval;
  }

  int SpinTrylock(pthread_spinlock_t * spinner) {
    int retval = -1;
    DEBUG("SpinTryLock: %lu", my_tid);
    _stat_serial.Start();
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
        DEBUG("SpinTryLockAcq: %lu", my_tid);
        passToken();
        break;
      }      
    }
    _stat_serial.Pause();        
    return retval;
  }

  int SpinUnlock(pthread_spinlock_t * spinner) {
    int retval = -1;
    DEBUG("SpinUnlock: %lu", my_tid);
    waitForToken();
    retval = ppthread_spin_unlock(spinner);
    DEBUG("SpinLockRel: %lu", my_tid);
    passToken();
    return retval;
  }

  int SpinDestroy(pthread_spinlock_t * spinner) {
    return ppthread_spin_destroy(spinner);
  }

  ////////////////////////////////////////////////////////////////////////// RWlock
  int RwLockInit(pthread_rwlock_t * rwlock, const pthread_rwlockattr_t * attr) {
    return ppthread_rwlock_init(rwlock, attr);
  }

  int RdLock(pthread_rwlock_t * rwlock) {
    int retval = -1;
    DEBUG("RdLock: %lu", my_tid);
    _stat_serial.Start();
    while (true) {
      waitForToken();
      retval = ppthread_rwlock_tryrdlock(rwlock);
      // Any thread having token has the right to acquire the lock
      if (retval == EBUSY) {
        // If thread fails to acquire the lock, then pass the token immediately
        DEBUG("RdLockBusy: %lu", my_tid);
        passToken();
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("RdLockAcq: %lu", my_tid);
        passToken();
        break;
      }
    }
    _stat_serial.Pause();
    return retval;
  }

  int WrLock(pthread_rwlock_t * rwlock) {
    int retval = -1;
    DEBUG("WrLock: %lu", my_tid);
    _stat_serial.Start();
    while (true) {
      waitForToken();
      retval = ppthread_rwlock_trywrlock(rwlock);
      // Any thread having token has the right to acquire the lock
      if (retval == EBUSY) {
        // If thread fails to acquire the lock, then pass the token immediately
        DEBUG("WrLockBusy: %lu", my_tid);
        passToken();
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("WrLockAcq: %lu", my_tid);
        passToken();
        break;
      }
    }
    _stat_serial.Pause();
    return retval;
  }

  int TimedRdLock(pthread_rwlock_t * rwlock, const struct timespec * timeout) {
    // Not supported
    return 0;
  }

  int TimedWrLock(pthread_rwlock_t * rwlock, const struct timespec * timeout) {
    // Not supported
    return 0;
  }  

  int TryRdLock(pthread_rwlock_t * rwlock) {
    int retval = -1;
    DEBUG("TryRdLock: %lu", my_tid);
    _stat_serial.Start();
    while (true) {
      waitForToken();
      retval = ppthread_rwlock_tryrdlock(rwlock);
      // if trylock failed, we no longer check token, since we do not lock 
      if (retval == EBUSY) {
        DEBUG("TryRdLockBusy: %lu", my_tid);
        passToken();
        return EBUSY;
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("TryRdLockAcq: %lu", my_tid);
        passToken();
        break;
      }      
    }
    _stat_serial.Pause();        
    return retval;
  }

  int TryWrLock(pthread_rwlock_t * rwlock) {
    int retval = -1;
    DEBUG("TryWrLock: %lu", my_tid);
    _stat_serial.Start();
    while (true) {
      waitForToken();
      retval = ppthread_rwlock_trywrlock(rwlock);
      // if trylock failed, we no longer check token, since we do not lock 
      if (retval == EBUSY) {
        DEBUG("TryWrLockBusy: %lu", my_tid);
        passToken();
        return EBUSY;
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("TryWrLockAcq: %lu", my_tid);
        passToken();
        break;
      }      
    }
    _stat_serial.Pause();        
    return retval;
  }  

  int RwUnLock(pthread_rwlock_t * rwlock) {
    int retval = -1;
    DEBUG("RwUnlock: %lu", my_tid);
    waitForToken();
    retval = ppthread_rwlock_unlock(rwlock);
    DEBUG("RwLockRel: %lu", my_tid);
    passToken();
    return retval;
  }

  int RwLockDestroy(pthread_rwlock_t * rwlock) {
    return ppthread_rwlock_destroy(rwlock);
  }

  ///////////////////////////////////////////////////////////////////////////////// Cond
  int CondInit(pthread_cond_t * cond, const pthread_condattr_t * attr) {
    return 0;
  }

  int CondWait(pthread_cond_t * cond, pthread_mutex_t * mutex) {
    return 0;
  }

  int CondSignal(pthread_cond_t * cond) {
    return 0;
  }

  int CondBroadcast(pthread_cond_t * cond) {
    return 0;
  }

  int CondDestroy(pthread_cond_t * cond) {
    return 0;
  }

  int BarrierInit(pthread_barrier_t * barrier, const pthread_barrierattr_t * attr, unsigned int count) {
    return 0;
  }

  int BarrierWait(pthread_barrier_t * barrier) {
    return 0;
  }

  int BarrierDestroy(pthread_barrier_t * barrier) {
    return 0;
  }


private:

  // Query an entry in the list by its index
  inline ThreadEntry * getThreadEntry(size_t _tid) {
    ThreadEntry * entry = NULL;
    // Locate the target thread in the entires
    std::vector<ThreadEntry>::size_type pos;
    for (pos=0; pos<_active_entries.size(); pos++) {
      if (_active_entries[pos].tid == _tid) {
        entry = &_active_entries[pos];
        break;   
      }
    }
    return entry;
  }

  // Busy waiting until the caller get the token
  inline void waitForToken(void) {
    DEBUG("WaitToken: %lu", my_tid);
    while (my_tid != _token_tid) {
      // FIXME: I am not sure whether the following instruction is necessary
      __asm__ __volatile__ ("mfence");
    }
    DEBUG("GetToken: %lu", my_tid);
  }

  // Force thread to pass his token to the next thread in the active list
  inline void passToken(void) {

    // Make sure only the token owner can pass token
    // FIXME: Can be removed in the release version.
    if (my_tid != _token_tid) {
      DEBUG("Error! <%lu> attempts to pass token but the token belongs to %lu", my_tid, _token_tid);
      assert(0);
      return;
    }

    // FIXME: Using the randomized passToken() will make spinlock() non-deterministic

    while (true) {
#ifdef RANDOM_NEXT
      _token_pos = (_token_pos + 1 + KISS % _active_entries.size()) % _active_entries.size();
#else
      _token_pos = (_token_pos + 1) % _active_entries.size();
#endif

      // Make sure the token is passing to a thread that does not own a lock
      if (_active_entries[_token_pos].owning_lock == false) break;
      
    }
    

    // Besides passToken, _token_pos is also modified when another thread is being deregisterred
    lock_();

    // Setup token id according to token position
    _token_tid = _active_entries[_token_pos].tid;
    DEBUG("PassToken: %lu", my_tid);

    unlock_();
    return;
  }

  inline void lock_(void) {
    ppthread_mutex_lock(&_mutex);
    return;
  }

  inline void unlock_(void) {
    ppthread_mutex_unlock(&_mutex);
    return;
  }


};


#endif
