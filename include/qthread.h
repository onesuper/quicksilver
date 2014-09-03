#ifndef _QTHREAD_H_
#define _QTHREAD_H_

#include <vector>
#include <cerrno>
#include <cstddef>
#include "ppthread.h" 
#include "debug.h"
#include "timer.h"
#include "random.h"


#define MAIN_TID 0

// We use this to track thread's status. If a thread is waken up by 
// any sync operation (e.g. cond signal/mutex release), we move its entry from sleep list
// to active list, or vice versa.
enum ThreadStatus {
  STATUS_READY,             // Different reasons for sleeping
  STATUS_LOCK_WAITING,      // The thread is waiting for a lock
  STATUS_COND_WAITING,      // The thread is waiting for a condition to become true
  STATUS_BARR_WAITING,      // The thread is waiting for other threads to pass the barrier
  STATUS_HIBERNATE,         // The thread will not enter critical section for a long time
};

// Pack all the parameters in a struct
class ThreadParam {
public:
  volatile ThreadFunction func;  
  volatile size_t tid;
  volatile void * arg;

  ThreadParam(): func(NULL), tid(0), arg(NULL) {}
};


// The parameter we need the thread to have. All threads being spawned share one copy
ThreadParam thread_param;


// Fake entry point of thread. We use it to unregister thread inside the thread body
void * fake_thread_entry(void *);


// Thread local storage: let each thread know who he is
__thread size_t my_tid;

// How soon can a lock be owned?
//#define LOCK_BUDGET 3

// Used to keep the safety of parameters passed to each spawned thread 
pthread_mutex_t spawn_lock;

// This is to switch-off pthread API, before Qthread's ctor is done
static bool qthread_initialized = false;


class Qthread {

private:

  // We will construct the instance in ctor
  static Qthread instance;

  ///////////////////////////////////////////////////////////////////// ThreadEntry  

  class ThreadEntry {
  public:
    volatile size_t tid;     // The thread we assigned
    volatile pthread_t pid;  // Assigned by pthread_self
    volatile int status;
    volatile void * cond;    // each thread may wait for one cond each time   
    volatile void * lock;
    // volatile void * barrier;
    // volatile bool broadcast; // If broadcast = true, the thread will be woken up by broadcast

    inline ThreadEntry() {}

    inline ThreadEntry(size_t tid, pthread_t pid) {
      this->tid = tid;
      this->pid = pid;
      this->status = STATUS_READY;
      this->cond = NULL;
      this->lock = NULL;
      // this->barrier = NULL;
    } 

    inline ThreadEntry(const ThreadEntry & entry) {
      this->tid = entry.tid;
      this->pid = entry.pid;
      this->status = entry.status;
      this->cond = entry.cond;
      this->lock = entry.lock;
      // this->barrier = entry.barrier;
    }
  };


#if 0
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
#endif


  // Maintain all the active thread entries in a vector
  // All threads in this list is participating the token passing game
  std::vector<ThreadEntry> _active_entries;

  // We will hold the hibernated threads in another vector
  std::vector<ThreadEntry> _sleep_entries;

  // An increasing number used to assign unique thread id
  volatile size_t _thread_count;

  // Pointing to the thread entry who holds the toke 
  // NOTE: This can be -1 (no one has token). When equal -1, the value
  // in _token_tid is invalid. So each time we turn _token_pos to 0, we must update _token_tid
  volatile int _token_pos;

  // The thread id of the token owner
  volatile size_t _token_tid;

  // All threads use this lock to prevent from data race
  pthread_mutex_t _mutex;

  // Wall-clock (total elapsed time)
  Timer _stat_total;
  
  // Time consumed in waiting token
  Timer _stat_serial;   
  
  // Next random thread gets token 
  bool _random_next;

public:

  static inline Qthread& GetInstance(void) {
    return instance;
  }

  ///////////////////////////////////////////////////////////////////// Constructor  
  // Intially we register the main process (#0) in the activelist, and set up its tid
  // #0 is always registered the activelist for the whole lifetime of the program
  // #0 will get the token first, and the token will never be called back
  Qthread(bool random_next = false):
  _active_entries(),
  _sleep_entries(),
  _thread_count(1), 
  _token_pos(0),   
  _token_tid(MAIN_TID),
  _stat_total(),
  _stat_serial(),
  _random_next(random_next) // whether we use random_next strategy
  {

    // We must call this function, before we can use any pthread references
    init_pthread_reference();

    _active_entries.reserve(1024);

    // Register Main thread
    my_tid = MAIN_TID;
    registerThread(MAIN_TID, 0);
    DEBUG("Reg: %d\n", MAIN_TID);

    // set up the random number generator
    settable(12345,65435,34221,12345,9983651,95746118);

    ppthread_mutex_init(&spawn_lock, NULL);
    ppthread_mutex_init(&_mutex, NULL);
    _stat_total.Start();
    qthread_initialized = true;
  }


  ~Qthread() {

    qthread_initialized = false;    
    _stat_total.Pause();
    ppthread_mutex_destroy(&_mutex);
    ppthread_mutex_destroy(&spawn_lock);
    printf("Total Time: %ld\n", _stat_total.Total());
    printf("Serial Time: %ld @ %ld\n", _stat_serial.Total(), _stat_serial.Times()); 
  }



  //////////////////////////////////////////////////////////////////////////// Pthread Basics

  // Do nothing but make token can be passed through
  void DummySync(void) {
    waitForToken();
    DEBUG("# DumSync: %lu\n", my_tid);
    passToken();
    return;
  }

  //The followin API is for activat/deactivate thread externally
  int HibernateThread(size_t tid) {
    waitForToken();
    //DEBUG("# Hibernate(%lu): %lu\n", tid, my_tid);
    int retval = deactivateThread(tid, STATUS_HIBERNATE);
    passToken();
    return retval;
  }

  int WakeUpThread(size_t tid) {
    waitForToken();
    //("# WakeUp(%lu): %lu\n", tid, my_tid);
    int retval = activateThread(tid);
    passToken();
    return retval;
  }

  // A wrapper for calling through pthread id
  int HibernateThreadByPid(pthread_t pid) {
    waitForToken();
    ThreadEntry * entry = getActiveEntryByPid(pid);
    if (entry == NULL) {
      DEBUG("# Hibernate404: %lu\n", my_tid);
      return 1;
    }
    size_t tid = entry->tid;
    //DEBUG("# Hibernate(%lu): %lu\n", tid, my_tid);
    int retval = deactivateThread(tid, STATUS_HIBERNATE);
    passToken();
    return retval;
  }
 
  int WakeUpThreadByPid(pthread_t pid) {
    waitForToken();
    ThreadEntry * entry = getActiveEntryByPid(pid);
    if (entry == NULL) { 
      DEBUG("# WakeUp404: %lu\n", my_tid);
      return 1;
    }
    size_t tid = entry->tid;
    //DEBUG("# WakeUp(%lu): %lu\n", tid, my_tid);
    int retval = activateThread(tid);
    passToken();
    return retval;
  }


  // Spawing is treated as a sync point as well
  int Spawn(pthread_t * pid, const pthread_attr_t * attr, ThreadFunction func, void * arg) {
    
    waitForToken();
    // This lock is just for the safety of 'thread_param' (global variable).
    // NOTE: this lock will make the initialization of threads be *serialized*. When we have a lot of 
    // threads to spawn, that may hurt performance. Try to replace it with a better mechanism later.
    ppthread_mutex_lock(&spawn_lock);

    size_t tid = getUniqueIndex();

    DEBUG("# Spawn(%lu): %lu\n", tid, my_tid);

    // Hook up the thread function and arguments
    thread_param.func = func;
    thread_param.arg = arg;
    thread_param.tid = tid;

    // The unlock of spawn_lock is located in the ThreadFuncWrapper we passed to ppthread_create()
    int retval = ppthread_create(pid, attr, fake_thread_entry, &thread_param);
    // ppthread_create may use lock internally and lose the token
    waitForToken();
    passToken();

    // IMPORTANT: We treat as another sync point 
    // Register after we have created the thread. We also record the pthread_id
    waitForToken();
    registerThread(tid, *pid);
    passToken();

    return retval;
  }

  // Call before a thread quit
  void Terminate(void) {
    waitForToken();
    deregisterThread(my_tid);
    passToken();
    return;
  }

  // pthread_join
  int Join(pthread_t tid, void ** val) {
    int retval = ppthread_join(tid, val);
    return retval;
  }

  // pthread_exit
  int Exit(void * value_ptr) {
    Terminate();
    return ppthread_exit(value_ptr);
  }

  //////////////////////////////////////////////////////////////////////////// Mutex
  // FIXME: If we do not wrap the mutex structure, this wrapper function can be removed
  // So as to MutexDestory(), SpinlockInit(), SpinlockDestroy()...
  int MutexInit(pthread_mutex_t * mutex, const pthread_mutexattr_t * attr) {
    return ppthread_mutex_init(mutex, attr);
  }

  // The following pair implements the wait version of mutex lock/unlock
  // In this version, if a thread want to acquire a lock, it will possibly leave the token passing game 
  // The determinsim is ensured by enqueueing the thread into the sleep list in order
  int LockAcquire(pthread_mutex_t * lock) {
    int retval = -1;

    //DEBUG("# LockAcq...: %lu\n", my_tid);

    waitForToken();

    // we attempts to acquire that lock (try one time)
    retval = ppthread_mutex_trylock(lock);

    if (retval != EBUSY) {
      // Acquired the lock
      passToken();
    } else {
      // If thread fails to acquire the lock, we then move it to the sleep entries and pass the token
      //DEBUG("# LockBusy: %lu\n", my_tid);

      bool isFound = false;
      for (unsigned int i = 0; i < _active_entries.size(); i++) {
        isFound = true;
        DEBUG("# LockSleep(%p): %lu\n", lock, my_tid);
        ThreadEntry entry = _active_entries[i];  // Deep copy
        entry.status = STATUS_LOCK_WAITING;     // mutex wait
        entry.lock = (void *) lock;
        _sleep_entries.push_back(entry);
        _active_entries.erase(_active_entries.begin() + i);
        // NOTE: here we just adjust token pos back and do not change _token_tid
        if (_active_entries.empty()) {
          _token_pos = -1;
          DEBUG("# CallbackToken: %lu\n", my_tid);
        } else {
          _token_pos = (_token_pos - 1) % _active_entries.size();  // Roll back the pos
        }
        break;
      }
      assert(isFound == true);

      passToken();


      while (true) {
        // Block until I am woken up by lock release. 
        ppthread_mutex_lock(lock);

        // Acquired anyway
        DEBUG("# LockAnyway(%p): %lu\n", lock, my_tid);

        // Check whether the thread is activated. 
        // If a thread is activated, we can find it in the active entry
        ThreadEntry * entry = getActiveEntry(my_tid);
        if (entry == NULL) {  
          // He is not the first waiter, so yield that lock
          DEBUG("# LockYield(%p): %lu\n", lock, my_tid);
          ppthread_mutex_unlock(lock);  
        } else { 
          break;
        }
      }
    }

    // If we arrive here, this thread must have *really* acquired the lock
    DEBUG("# LockAcq(%p): %lu\n", lock, my_tid);

    return retval;
  }


  // Wake up the first thread (who is wait the lock )in the sleep entries 
  int LockRelease(pthread_mutex_t * lock) {
    int retval = -1;

    //DEBUG("# ReleaseLock(%p): %lu\n", lock, my_tid);

    waitForToken();

    bool isFound = false;

    for (unsigned int i = 0; i < _sleep_entries.size(); i++) {
      ThreadEntry entry = _sleep_entries[i];
      if (entry.lock == (void *) lock) {
        isFound = true;
        DEBUG("# LockWakeup(%lu): %lu\n", entry.tid, my_tid);
        entry.status = STATUS_READY;   // set ready
        entry.lock = NULL;
        _active_entries.push_back(entry);  
        _sleep_entries.erase(_sleep_entries.begin() + i);
        break;   
      }
    }

    // unlock may wakeup other threads waiting on that lock
    // we resolve this problem in when acquiring the lock
    ppthread_mutex_unlock(lock); 
    DEBUG("# LockRel(%p): %lu\n", lock, my_tid);


    // If some thread is blocked for this lock, we just pass token to him
    if (isFound == true) {
        // Pass token to the thread that should be woken up
      _token_pos = _active_entries.size() - 1;
      _token_tid = _active_entries[_token_pos].tid;
      DEBUG("# throwToken(%lu): %lu\n", _token_tid, my_tid);
    } else {
      passToken();
    }


    return retval;
  }


  int MutexLock(pthread_mutex_t * mutex) {
    int retval = -1;

    //DEBUG("# MutexLock: %lu\n", my_tid);

    while (true) { // As long as retval==EBUSY, the loop will go on
      waitForToken();
      retval = ppthread_mutex_trylock(mutex);
      // Any thread having token has the right to acquire the lock
      if (retval == EBUSY) {
        // If thread fails to acquire the lock, then pass the token immediately
        // This prevent the case that thread sleep on the mutex while holding token
        DEBUG("# MutexLockBusy: %lu\n", my_tid);
        passToken();
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("# MutexLockAcq(%p): %lu\n", mutex, my_tid); // acquire
        passToken();
        break;
      }
    }
    return retval;
  }

  int MutexTrylock(pthread_mutex_t * mutex) {
    int retval = -1;
    //DEBUG("# MutexTryLock: %lu\n", my_tid);
    while (true) {
      waitForToken();          
      // Acqurie the lock anyway, let other competitors sleep
      retval = ppthread_mutex_trylock(mutex);

      // This is a trylock, so we do not desire for locks
      // If trylock failed, just return directly
      if (retval == EBUSY) {
        DEBUG("# MutexTryLockBusy: %lu\n", my_tid);
        passToken();
        return EBUSY;  
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("# MutexTryLockAcq: %lu\n", my_tid);
        passToken();
        break;
      }
    }

    return retval;
  }

  // In this version, each thread acquires the lock anyway before
  // checking the token ownership. If it doesn't own token just yield it 
  // and repeat to acuquire the lock and then check the token...
  int MutexWaitLock(pthread_mutex_t * mutex) {
    int retval = -1;

    //DEBUG("# MutexWaitLock: %lu\n", my_tid);
  
    while (true) {
      retval = ppthread_mutex_lock(mutex);
      // Any thread may get the lock
      DEBUG("# MutexLockAcq: %lu\n", my_tid);
      // Check whether I have the token
      if (my_tid != _token_tid) {
        // if I have no token, yield the lock
        DEBUG("# MutexLockYield: %lu\n", my_tid);
        ppthread_mutex_unlock(mutex); 
      } else {
        break;
      }
    }
    // Lock acquired here. Pass token and continues its execution
    passToken(); 
    return retval;
  }

  int MutexUnlock(pthread_mutex_t * mutex) {
    int retval = -1;
    //DEBUG("# MutexUnlock: %lu\n", my_tid);
    waitForToken();
    retval = ppthread_mutex_unlock(mutex);
    DEBUG("# MutexLockRel(%p): %lu\n", mutex, my_tid);
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
   // DEBUG("# SpinLock: %lu\n", my_tid);
    while (true) {
      waitForToken();
      retval = ppthread_spin_trylock(spinner);
      // Any thread having token has the right to acquire the lock
      if (retval == EBUSY) {
        // If thread fails to acquire the lock, then pass the token immediately
        DEBUG("# SpinLockBusy: %lu\n", my_tid);
        passToken();
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("# SpinLockAcq: %lu\n", my_tid);
        passToken();
        break;
      }
    }
    return retval;
  }

  int SpinTrylock(pthread_spinlock_t * spinner) {
    int retval = -1;
    //DEBUG("# SpinTryLock: %lu\n", my_tid);
    while (true) {
      waitForToken();
      retval = ppthread_spin_trylock(spinner);
      // if trylock failed, we no longer check token, since we do not lock 
      if (retval == EBUSY) {
        DEBUG("# SpinTryLockBusy: %lu\n", my_tid);
        passToken();
        return EBUSY;
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("# SpinTryLockAcq: %lu\n", my_tid);
        passToken();
        break;
      }      
    }
    return retval;
  }

  int SpinUnlock(pthread_spinlock_t * spinner) {
    int retval = -1;
    DEBUG("# SpinUnlock: %lu\n", my_tid);
    waitForToken();
    retval = ppthread_spin_unlock(spinner);
    DEBUG("# SpinLockRel: %lu\n", my_tid);
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
    //DEBUG("# RdLock: %lu \n", my_tid);
    while (true) {
      waitForToken();
      retval = ppthread_rwlock_tryrdlock(rwlock);
      // Any thread having token has the right to acquire the lock
      if (retval == EBUSY) {
        // If thread fails to acquire the lock, then pass the token immediately
        DEBUG("# RdLockBusy(%p): %lu\n", rwlock, my_tid);
        passToken();
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("# RdLockAcq(%p): %lu\n", rwlock, my_tid);
        passToken();
        break;
      }
    }
    return retval;
  }

  int WrLock(pthread_rwlock_t * rwlock) {
    int retval = -1;
    //DEBUG("# WrLock: %lu\n", my_tid);
    while (true) {
      waitForToken();
      retval = ppthread_rwlock_trywrlock(rwlock);
      // Any thread having token has the right to acquire the lock
      if (retval == EBUSY) {
        // If thread fails to acquire the lock, then pass the token immediately
        DEBUG("# WrLockBusy(%p): %lu\n", rwlock, my_tid);
        passToken();
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("# WrLockAcq(%p): %lu\n", rwlock, my_tid);
        passToken();
        break;
      }
    }
    return retval;
  }

  int RdTryLock(pthread_rwlock_t * rwlock) {
    int retval = -1;
    //DEBUG("# TryRdLock: %lu\n", my_tid);
    while (true) {
      waitForToken();
      retval = ppthread_rwlock_tryrdlock(rwlock);
      // if trylock failed, we no longer check token, since we do not lock 
      if (retval == EBUSY) {
        DEBUG("# TryRdLockBusy(%p): %lu\n", rwlock, my_tid);
        passToken();
        return EBUSY;
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("# TryRdLockAcq(%p): %lu\n", rwlock, my_tid);
        passToken();
        break;
      }      
    }
    return retval;
  }

  int WrTryLock(pthread_rwlock_t * rwlock) {
    int retval = -1;
    //DEBUG("# TryWrLock: %lu\n", my_tid);
    while (true) {
      waitForToken();
      retval = ppthread_rwlock_trywrlock(rwlock);
      // if trylock failed, we no longer check token, since we do not lock 
      if (retval == EBUSY) {
        DEBUG("# TryWrLockBusy(%p): %lu\n", rwlock ,my_tid);
        passToken();
        return EBUSY;
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("# TryWrLockAcq(%p): %lu\n", rwlock, my_tid);
        passToken();
        break;
      }      
    }
    return retval;
  }  

  int RwUnlock(pthread_rwlock_t * rwlock) {
    int retval = -1;
    //DEBUG("# RwUnlock: %lu\n", my_tid);
    waitForToken();
    retval = ppthread_rwlock_unlock(rwlock);
    DEBUG("# RwLockRelease(%p): %lu\n", rwlock, my_tid);
    passToken();
    return retval;
  }

  int RwLockDestroy(pthread_rwlock_t * rwlock) {
    return ppthread_rwlock_destroy(rwlock);
  }

  ///////////////////////////////////////////////////////////////////////////////// Cond

  int CondInit(pthread_cond_t * cond, const pthread_condattr_t * attr) {
    return ppthread_cond_init(cond, attr);
  }

  int CondWait(pthread_cond_t * cond, pthread_mutex_t * cond_mutex) {
    int retval = -1;

    // Before we sleep, we move the entry to sleep list, so that my sleep
    // will not affect the token passing game
    // The order entering the sleep entry is ensured by token
    waitForToken();
    bool isFound = false;
    // Deactive myself
    for (unsigned int i = 0; i < _active_entries.size(); i++) {
      if (_active_entries[i].tid == my_tid) {
        isFound = true;
        DEBUG("# CondSleep(%p): %lu\n", cond, my_tid);
        ThreadEntry entry = _active_entries[i]; // Deep copy
        entry.status = STATUS_COND_WAITING;     // Cond wait
        entry.cond = (void *) cond;
        _sleep_entries.push_back(entry);
        _active_entries.erase(_active_entries.begin() + i);
        // NOTE: here we just adjust token pos back and do not change _token_tid
        if (_active_entries.empty()) {
          _token_pos = -1;
          DEBUG("# CallbackToken: %lu\n", my_tid);
        } else {
          _token_pos = (_token_pos - 1) % _active_entries.size();  // Roll back the pos
        }
        break;
      }
    }
    assert(isFound == true);
    passToken();


    // Block until I become active. (leave token passing)
    // The order threads leave this loop is determinisitic
    // FIXME: getActiveEntry() is not thread-safe! And it requires scanning each time
    while (true) {
      // Suspend on the real cond. 
      // DEBUG("# CondLockRel(%p): %lu\n", cond_mutex, my_tid);
      retval = ppthread_cond_wait(cond, cond_mutex);
      // DEBUG("# CondLockAcq(%p): %lu\n", cond_mutex, my_tid);

      DEBUG("# CondWokenUp(%p): %lu\n", cond, my_tid);
      // Re-evaluate after being woken up
      // Progress only if the thread is activated 
      // FIXME: Scanning is time-comsuming! 
      ThreadEntry * entry = getActiveEntry(my_tid);
      if (entry == NULL) { // But I am not the one, I should signal again
        DEBUG("# CondSignalOther(%p): %lu\n", cond, my_tid);
        ppthread_cond_signal(cond);
      } else {  
        break;
      }  
    }
    

    DEBUG("# CondOK(%p): %lu\n", cond, my_tid);
    return retval;
  }


  int CondSignal(pthread_cond_t * cond) {
    int retval = -1;

    waitForToken();


    bool isFound = false;

    // Look for the first cond waiting thread in the sleep entry
    // And move it back to the active entry
    for (unsigned int i = 0; i < _sleep_entries.size(); i++) {
      ThreadEntry entry = _sleep_entries[i];  // Deep copy
      if (entry.cond == (void *) cond) {
        isFound = true;
        // Activate thread
        DEBUG("# CondActivate(%lu): %lu\n", entry.tid, my_tid);
        entry.status = STATUS_READY;   // set ready
        entry.cond = NULL;
        // entry->broadcast = false;
        _active_entries.push_back(entry);  
        _sleep_entries.erase(_sleep_entries.begin() + i);
        break;   
      }
    }

    DEBUG("# CondSignal(%p): %lu\n", cond, my_tid);
    retval = ppthread_cond_signal(cond);
    
    // Pass token to the thread that should be woken up
    if (isFound) {
      _token_pos = _active_entries.size() - 1;
      _token_tid = _active_entries[_token_pos].tid;
      DEBUG("# throwToken(%lu): %lu\n", _token_tid, my_tid);
    } else {
      // no one is waiting on this cond
      passToken();
    }


    return retval;
  }

  int CondBroadcast(pthread_cond_t * cond) {
    int retval = -1;

    waitForToken();

    int first_token_pos = -1;

    // Wake up all the corresponding threads
    for (unsigned int i = 0; i < _sleep_entries.size(); ) {
      ThreadEntry entry = _sleep_entries[i]; // Deep cocy
      if (entry.cond == (void *) cond) {
        DEBUG("# CondActivate(%lu): %lu\n", entry.tid, my_tid);
        entry.status = STATUS_READY;   // set ready
        entry.cond = NULL;
        _active_entries.push_back(entry);
        _sleep_entries.erase(_sleep_entries.begin() + i);

        // Assign the token pos to the thread which will wakeup firstly
        if (first_token_pos = -1) {
          first_token_pos = _active_entries.size() - 1;
        }

      } else {
        i++; 
      }
    }

    DEBUG("# CondBroadcast(%p): %lu\n", cond, my_tid);
    retval = ppthread_cond_broadcast(cond);

    // Pass token to the thread that should be woken up (firstly)
    if (first_token_pos != -1) {
      _token_pos = first_token_pos;
      _token_tid = _active_entries[_token_pos].tid;
      DEBUG("# throwToken(%lu): %lu\n", _token_tid, my_tid);
    } else {
      // No thread is waiting for that cond
      passToken();
    }

    return retval;
  }

  int CondDestroy(pthread_cond_t * cond) {
    return ppthread_cond_destroy(cond);
  }

  ///////////////////////////////////////////////////////////////////////////////// Barrier

  int BarrierInit(pthread_barrier_t * barrier, const pthread_barrierattr_t * attr, unsigned int count) {
    return 0;
  }

  int BarrierWait(pthread_barrier_t * barrier) {
    return 0;
  }

  int BarrierDestroy(pthread_barrier_t * barrier) {
    return 0;
  }


  // For testing
  int Blah(void) {

    DEBUG("\n* Token * ");
    DEBUG("%lu [%d]", _token_tid, _token_pos);

    DEBUG("\n* Active * ");
    for (unsigned int i = 0; i < _active_entries.size(); i++) {
      DEBUG("%lu   ", _active_entries[i].tid);
    }

    DEBUG("\n* Sleep * ");
    for (unsigned int i = 0; i < _sleep_entries.size(); i++) {
      DEBUG("%lu   ", _sleep_entries[i].tid);
    }
    DEBUG("\n* * * \n");

  }


private:


  ///////////////////////////////////////////////////////////////////// Registeration
  // For any threads we want determinsitic execution, we can this function to register it
  // NOTE: Thread to call the following registeration function must pocess token token
  int registerThread(size_t tid, pthread_t pid) {

    // Check duplication in active_entries
    for (unsigned int i = 0; i < _active_entries.size(); i++) {
      if (_active_entries[i].tid == tid) {
        DEBUG("RegDuplicate(%lu): %lu\n", tid, my_tid);
        return 1;
      }
    }
    DEBUG("# Regsiter(%lu): %lu\n", tid, my_tid);
    ThreadEntry entry(tid, pid);
    entry.status = STATUS_READY;
    _active_entries.push_back(entry);

    // Start from a empty list
    if (_token_pos == -1) {
      _token_pos = 0;
      _token_tid = tid;
      DEBUG("# StartToken(%lu): %lu\n", tid, my_tid);
    }
    return 0;
  }


  int deregisterThread(size_t tid) {
    // Locate the target thread in the entires
    for (unsigned int i = 0; i < _active_entries.size(); i++) {
      if (_active_entries[i].tid == tid) {
        DEBUG("# Deregister(%lu): %lu\n", tid, my_tid);
        _active_entries.erase(_active_entries.begin() + i);

        // NOTE: here we just adjust token pos back and do not change _token_tid
        if (_active_entries.empty()) {
          _token_pos = -1;
          DEBUG("# CallbackToken: %lu\n", my_tid);
        } else { 
          _token_pos = (_token_pos - 1) % _active_entries.size();  // Roll back the pos
        }
        return 0;
      }
    }
    // if  no entry is found
    DEBUG("# Deregister404(%lu): %lu\n", tid, my_tid);
    return 1;
  }


  // Move a specific thread from _active_entries to _sleep_entrires
  // Any sleeping threads must be assigned a status
  int deactivateThread(size_t tid, int status) {
    // Locate the target thread in the entires
    for (unsigned int i = 0; i < _active_entries.size(); i++) {
      if (_active_entries[i].tid == tid) {
        DEBUG("# Deactivate(%lu): %lu\n", tid, my_tid);
        ThreadEntry entry = _active_entries[i]; // Deep copy
        entry.status = status;
        _sleep_entries.push_back(entry);
        _active_entries.erase(_active_entries.begin() + i);
        // NOTE: here we just adjust token pos back and do not change _token_tid
        if (_active_entries.empty()) {
          _token_pos = -1;
          DEBUG("# CallbackToken: %lu\n", my_tid);
        } else {
          _token_pos = (_token_pos - 1) % _active_entries.size();  // Roll back the pos
        }
        return 0;
      }
    }
    // if  no entry is found
    DEBUG("# Deactivate404(%lu): %lu\n", tid, my_tid);
    return 1;
  }


  int activateThread(size_t tid) {
    // Locate the target thread in the entires
    for (unsigned int i = 0; i < _sleep_entries.size(); i++) {
      if (_sleep_entries[i].tid == tid) {
        DEBUG("# Activate: %lu\n", tid);
        ThreadEntry entry = _sleep_entries[i]; // Deep copy
        entry.status = STATUS_READY;
        _active_entries.push_back(entry);
        _sleep_entries.erase(_sleep_entries.begin() + i);
        // Start from a empty list
        if (_token_pos == -1) {
          _token_pos = 0;
          _token_tid = tid;
          DEBUG("# StartToken(%lu): %lu\n", tid, my_tid);
        }
        return 0;
      }
    }
    // if  no entry is found
    DEBUG("# Activate404: %lu\n", tid);
    return 1;
  }


  // Call gcc atomic operation to assign an increasing unique number
  inline size_t getUniqueIndex(void) {
    return __sync_fetch_and_add(&_thread_count, 1);
  }

  // Query an entry in the list by its index
  // Return NULL if not entry is found
  inline ThreadEntry * getActiveEntry(size_t tid) {
    ThreadEntry * entry = NULL;
    // Locate the target thread in the entires
    for (unsigned int i = 0; i < _active_entries.size(); i++) {
      if (_active_entries[i].tid == tid) {
        entry = &_active_entries[i];
        break;   
      }
    }
    return entry;
  }

  // Query an entry in the list by its index
  // Return NULL if not entry is found
  inline ThreadEntry * getSleepEntry(size_t tid) {
    ThreadEntry * entry = NULL;
    // Locate the target thread in the entires
    for (unsigned int i = 0; i < _sleep_entries.size(); i++) {
      if (_sleep_entries[i].tid == tid) {
        entry = &_sleep_entries[i];
        break;   
      }
    }
    return entry;    
  }

  // Query an entry in the list by its index
  // Return NULL if not entry is found
  inline ThreadEntry * getActiveEntryByPid(pthread_t pid) {
    ThreadEntry * entry = NULL;
    // Locate the target thread in the entires
    for (unsigned int i = 0; i < _active_entries.size(); i++) {
      if (_active_entries[i].pid == pid) {
        entry = &_active_entries[i];
        break;   
      }
    }
    return entry;
  }

  // Query an entry in the list by its index
  // Return NULL if not entry is found
  inline ThreadEntry * getSleepEntryByPid(pthread_t pid) {
    ThreadEntry * entry = NULL;
    // Locate the target thread in the entires
    for (unsigned int i = 0; i < _sleep_entries.size(); i++) {
      if (_sleep_entries[i].pid == pid) {
        entry = &_sleep_entries[i];
        break;   
      }
    }
    return entry;
  }


  // Busy waiting until the caller get the token
  inline void waitForToken(void) const {

    if (_active_entries.empty()) return;
    //assert(!_active_entries.empty());

    DEBUG("# WaitToken: %lu\n", my_tid);
    while (my_tid != _token_tid) {
      __asm__ __volatile__ ("mfence");
    }
    //DEBUG("# GetToken: %lu\n", my_tid);
  }

  // Force token holder yield the token to the next thread in the active list
  // The calling thread doesn't have to be the token holder
  inline void passToken(void) {

    assert(_token_tid == my_tid);

    if (_active_entries.empty()) {
      _token_pos = -1;
      DEBUG("# CallbackToken: %lu\n", my_tid);
    } else {
      // IMPORTANT: Using the randomized passToken() will make spinlock() non-deterministic
      if (_random_next) {
        _token_pos = (_token_pos + 1 + KISS % _active_entries.size()) % _active_entries.size();
      } else {
        _token_pos = (_token_pos + 1) % _active_entries.size();
      }
      _token_tid = _active_entries[_token_pos].tid;
      DEBUG("# PassToken(->%lu): %lu\n", _token_tid, my_tid);
    }
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



/**
 * Fake entry point of thread. We use it to unregister thread inside the thread body
 */
void * fake_thread_entry(void * param) {

  ThreadParam * obj = static_cast<ThreadParam *>(param);
  
  // Dump parameters
  ThreadFunction my_func = obj->func;
  void * my_arg = (void *) obj->arg;
  my_tid = obj->tid;  

  // Unlock after copying out paramemters
  ppthread_mutex_unlock(&spawn_lock);

  // Call the real thread function
  void * retval = my_func(my_arg);

  // Let each thread deregister it self
  Qthread::GetInstance().Terminate();

  return retval;
}


#endif
