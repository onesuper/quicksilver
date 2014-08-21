#ifndef _QTHREAD_H_
#define _QTHREAD_H_

#include <vector>
#include <cerrno>
#include <cstddef>
#include "ppthread.h" 
#include "debug.h"
#include "timer.h"
#include "random.h"


// Pack all the parameters in a struct
class ThreadParam {
public:
  volatile ThreadFunction func;  
  volatile size_t index;
  volatile void * arg;

  ThreadParam(): func(NULL), index(0), arg(NULL) {}
};


// The parameter we need the thread to have. All threads being spawned share one copy
ThreadParam thread_param;


// Fake entry point of thread. We use it to unregister thread inside the thread body
void * fake_thread_entry(void *);



#define INVALID_TID 0x7FFFFFFF

#define MAIN_TID 0

// Thread local storage: let each thread know who he is
__thread size_t my_tid = INVALID_TID;

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
    size_t tid;

    ThreadEntry() {}
    ThreadEntry(size_t _tid): tid(_tid) {}
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
  std::vector<ThreadEntry> _active_entries;

  // An increasing number used to assign unique thread id
  volatile size_t _thread_count;

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


    my_tid = MAIN_TID;
    RegisterThread(MAIN_TID);
    DEBUG("Reg: %d\n", MAIN_TID);

    // set up the random number generator
    settable(12345,65435,34221,12345,9983651,95746118);

    ppthread_mutex_init(&spawn_lock, NULL);
    ppthread_mutex_init(&_mutex, NULL);
    _stat_total.Start();

    qthread_initialized = true;
  }


  ~Qthread() {
    _stat_total.Pause();


    ppthread_mutex_destroy(&_mutex);
    ppthread_mutex_destroy(&spawn_lock);

    printf("Total Time: %ld\n", _stat_total.Total());
    printf("Serial Time: %ld @ %ld\n", _stat_serial.Total(), _stat_serial.Times()); 

    qthread_initialized = false;
  }


  ///////////////////////////////////////////////////////////////////// Registeration
  // For any threads we want determinsitic execution, we can this function to register it
  // RegisterMe() will set up a unique my_tid 
  int RegisterThread(size_t tid) {

    // If this is a newly created thread, we assign a unique index to him
    // The uniqueness of tid is ensured, so there must be no duplications in active_entries
    // We can add it directly
    waitForToken();

    // Otherwise, we must check duplication in active_entries
    for (unsigned int i = 0; i < _active_entries.size(); i++) {
      if (_active_entries[i].tid == tid) {
        DEBUG("RegDup: %lu\n", tid);
        passToken();
        return 1;
      }
    }

    DEBUG("# Regster: %lu\n", tid);
    // Create a new entry according to tid    
    ThreadEntry entry(tid);

    // Push the newly created threadEntry into the active entries
    _active_entries.push_back(tid);

#if 0
    printf("Entries: ");
    for (int i=0; i<_active_entries.size(); i++) {
      printf("%lu,", _active_entries[i].tid);
    }
    printf("\n");
    printf("Token: %lu @ [%d]\n", ((_token_pos >= 0 ) ? _token_tid : INVALID_TID), _token_pos);
#endif

    passToken();
    return 0;
  }


  // We do not treat thread deregisteration as a sync point
  // So it must be protected by lock
  int DeregisterMe(void) {

    // Main thread is not allowed to quit game
    //assert(my_tid != MAIN_TID);

    waitForToken();

    // 0. First Locate the target thread in the entires
    unsigned int pos_to_del, i;
    for (i = 0; i < _active_entries.size(); i++) {
      if (_active_entries[i].tid == my_tid) {
        pos_to_del = i;
        goto EntryFound;   
      }
    }
    // If we arrive here, then no entry is found
    DEBUG("# DeReg404: %lu\n", my_tid);
    passToken();
    return 1;

EntryFound:
    DEBUG("# DeReg: %lu\n", my_tid);

    // Delete the entry from the active list. 
    _active_entries.erase(_active_entries.begin() + pos_to_del);

    if (_active_entries.empty()) {
      passToken();
      return 0;
    }

    // If the pos to delete is above the token pos, deleting it will affect the token pointer
    // So we have to adjust it back
    if (pos_to_del < (unsigned int) _token_pos) {
      _token_pos--;
    } 
    // Otherwise, deleting entires[pos] will not affect the token ownership 
    // The next entry gets the token for free (_token_pos doesn't move)

    // loop back to the first position
    if (pos_to_del == _active_entries.size()) { 
      _token_pos = 0;
    }

    // Reset token_tid according to the latest token_pos
    _token_tid = _active_entries[_token_pos].tid;
    return 0;
  }


  // Create is treated as a sync point as well
  int Create(pthread_t * tid, const pthread_attr_t * attr, ThreadFunction func, void * arg) {
    
    waitForToken();

    // This lock is just for the safety of 'thread_param' (global variable).
    // TODO: this lock will make the initialization of threads be *serialized*.
    // When we have a lot of threads to spawn, that may hurt performance.
    // Try to replace it with a better mechanism later.
    ppthread_mutex_lock(&spawn_lock);

    size_t index = getUniqueIndex();

    DEBUG("# Spawn%lu: %lu\n", index, my_tid);

    // Hook up the thread function and arguments
    thread_param.func = func;
    thread_param.arg = arg;
    thread_param.index = index;

    // The unlock of spawn_lock is located in the ThreadFuncWrapper we passed to ppthread_create()
    int retval = ppthread_create(tid, attr, fake_thread_entry, &thread_param);
    // ppthread_create may call internal lock and lose the token
    waitForToken();
    
    passToken();

    RegisterThread(index);

    return retval;

  }

  //////////////////////////////////////////////////////////////////////////// Mutex
  // FIXME: If we do not wrap the mutex structure, this wrapper function can be removed
  // So as to MutexDestory(), SpinlockInit(), SpinlockDestroy()...
  int MutexInit(pthread_mutex_t * mutex, const pthread_mutexattr_t * attr) {
    return ppthread_mutex_init(mutex, attr);
  }

  int MutexLock(pthread_mutex_t * mutex) {
    int retval = -1;
    DEBUG("# MutexLock: %lu\n", my_tid);
    _stat_serial.Start();

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
    _stat_serial.Pause();        
    return retval;
  }


  int MutexTrylock(pthread_mutex_t * mutex) {
    int retval = -1;
    DEBUG("# MutexTryLock: %lu\n", my_tid);
    _stat_serial.Start();
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
    _stat_serial.Pause();        
    return retval;
  }

  // Provide a wait lock version for pthread mutex lock
  // In this version, each thread acquires the lock anyway before
  // checking the token ownership. If it doesn't own token just yield it 
  // and repeat to acuquire the lock and then check the token...
  int MutexWaitLock(pthread_mutex_t * mutex) {
    int retval = -1;
    DEBUG("# MutexWaitLock: %lu\n", my_tid);
    _stat_serial.Start();
    while (true) {          
      // Acqurie the lock anyway, let other competitors sleep
      retval = ppthread_mutex_lock(mutex);
      DEBUG("# MutexLockAcq: %lu\n", my_tid);
      // Check whether I have the token
      if (my_tid != _token_tid) {
        // if I have no token, yield the lock
        DEBUG("# MutexLockYield: %lu\n", my_tid);
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
    DEBUG("# MutexUnlock: %lu\n", my_tid);
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
    DEBUG("# SpinLock: %lu\n", my_tid);
    _stat_serial.Start();
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
    _stat_serial.Pause();
    return retval;
  }

  int SpinTrylock(pthread_spinlock_t * spinner) {
    int retval = -1;
    DEBUG("# SpinTryLock: %lu\n", my_tid);
    _stat_serial.Start();
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
    _stat_serial.Pause();        
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
    DEBUG("# RdLock: %lu \n", my_tid);
    _stat_serial.Start();
    while (true) {
      waitForToken();
      retval = ppthread_rwlock_tryrdlock(rwlock);
      // Any thread having token has the right to acquire the lock
      if (retval == EBUSY) {
        // If thread fails to acquire the lock, then pass the token immediately
        DEBUG("# RdLockBusy: %lu\n", my_tid);
        passToken();
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("# RdLockAcq: %lu\n", my_tid);
        passToken();
        break;
      }
    }
    _stat_serial.Pause();
    return retval;
  }

  int WrLock(pthread_rwlock_t * rwlock) {
    int retval = -1;
    DEBUG("# WrLock: %lu\n", my_tid);
    _stat_serial.Start();
    while (true) {
      waitForToken();
      retval = ppthread_rwlock_trywrlock(rwlock);
      // Any thread having token has the right to acquire the lock
      if (retval == EBUSY) {
        // If thread fails to acquire the lock, then pass the token immediately
        DEBUG("# WrLockBusy: %lu\n", my_tid);
        passToken();
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("# WrLockAcq: %lu\n", my_tid);
        passToken();
        break;
      }
    }
    _stat_serial.Pause();
    return retval;
  }

  int RdTryLock(pthread_rwlock_t * rwlock) {
    int retval = -1;
    DEBUG("# TryRdLock: %lu\n", my_tid);
    _stat_serial.Start();
    while (true) {
      waitForToken();
      retval = ppthread_rwlock_tryrdlock(rwlock);
      // if trylock failed, we no longer check token, since we do not lock 
      if (retval == EBUSY) {
        DEBUG("# TryRdLockBusy: %lu\n", my_tid);
        passToken();
        return EBUSY;
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("# TryRdLockAcq: %lu\n", my_tid);
        passToken();
        break;
      }      
    }
    _stat_serial.Pause();        
    return retval;
  }

  int WrTryLock(pthread_rwlock_t * rwlock) {
    int retval = -1;
    DEBUG("# TryWrLock: %lu\n", my_tid);
    _stat_serial.Start();
    while (true) {
      waitForToken();
      retval = ppthread_rwlock_trywrlock(rwlock);
      // if trylock failed, we no longer check token, since we do not lock 
      if (retval == EBUSY) {
        DEBUG("# TryWrLockBusy: %lu\n", my_tid);
        passToken();
        return EBUSY;
      } else {
        // If we arrive here, this thread must have acquired the lock
        DEBUG("# TryWrLockAcq: %lu\n", my_tid);
        passToken();
        break;
      }      
    }
    _stat_serial.Pause();        
    return retval;
  }  

  int RwUnlock(pthread_rwlock_t * rwlock) {
    int retval = -1;
    DEBUG("# RwUnlock: %lu\n", my_tid);
    waitForToken();
    retval = ppthread_rwlock_unlock(rwlock);
    DEBUG("# RwLockRel: %lu\n", my_tid);
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

  // Call gcc atomic operation to assign an increasing unique number
  inline size_t getUniqueIndex(void) {
    return __sync_fetch_and_add(&_thread_count, 1);
  }

  // Query an entry in the list by its index
  // Return NULL if not entry is found
  ThreadEntry * getThreadEntry(size_t tid) {
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

  // Busy waiting until the caller get the token
  inline void waitForToken(void) const {
    DEBUG("# WaitToken: %lu\n", my_tid);
    while (my_tid != _token_tid) {
      // FIXME: I am not sure whether the following instruction is necessary
      __asm__ __volatile__ ("mfence");
    }
    DEBUG("# GetToken: %lu\n", my_tid);
  }

  // Force thread to pass his token to the next thread in the active list
  inline void passToken(void) {


    if (_active_entries.empty()) return;

    // Make sure only the token owner can pass token
    // FIXME: Can be removed in the release version.
    assert(my_tid == _token_tid);


    DEBUG("# PassToken: %lu\n", my_tid);

    // FIXME: Using the randomized passToken() will make spinlock() non-deterministic
    // Because the times calling spinlock() is dependent on timing
    if (_random_next) {
      _token_pos = (_token_pos + 1 + KISS % _active_entries.size()) % _active_entries.size();
    } else {
      _token_pos = (_token_pos + 1) % _active_entries.size();
    }
  
    // Reset token id according to token position
    // Since we use modelar arithmetic, the access is safe
    _token_tid = _active_entries[_token_pos].tid;

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
