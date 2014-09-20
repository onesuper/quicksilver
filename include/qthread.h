//////////////////////////////////////////////////////////////////////////////////////////////////
//
// Project name: 
//       Deterministic Replay of Multi-threaded Programs (2014 Intern project)
// Project owner: 
//       Yichao Cheng (onesuperclark@gmail.com)
//       Cheng Li     (cli@synopsys.com)
// Started date: 
//       July 10, 2014
// last Update:
//       September 30, 2014
//////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef _QTHREAD_H_
#define _QTHREAD_H_


// if defined we will log out the immediate information of the library
#define DEBUG_QTHREAD

//#define PROFILE_QTHREAD



#include <vector>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include "ppthread.h" 
#include "debug.h"
#include "timer.h"
#include "random.h"



///////////////////////////////////////////////////////////////////// Defines
#define TOKEN_OWNERSHIP_ON 1


// If defined, sleep entry works like a FIFO 
#define FIRST_SLEEP_FIRST_WOKENUP 1

// The main program also has a thread tid 
#define MAIN_TID 0

// If my_tid = INVALID_TID, all the sync operation will disabled
#define INVALID_TID 0x7fffffff


///////////////////////////////////////////////////////////////////// Fake thread entry/parameter
class ThreadParam {
public:
  ThreadFunction func;  
  size_t tid;
  void * arg;

  ThreadParam(): func(NULL), tid(0), arg(NULL) {}
};

// Fake entry point of thread. We use it to unregister thread inside the thread body

void * fake_thread_entry(void *);

// The parameter we need the thread to have. All threads being spawned share one copy
ThreadParam g_thread_param;

// Used to keep the safety of parameters passed to each spawned thread 
pthread_mutex_t g_spawn_lock;

///////////////////////////////////////////////////////////////////// Lock Ownership  

// Each thread can own one spin lock
__thread void * owned_spinner;
__thread unsigned int owned_spinner_budget;
__thread bool owned_spinner_is_locked;


// And one mutex lock
__thread void * owned_mutex;
__thread unsigned int owned_mutex_budget;
__thread bool owned_mutex_is_locked;

// Each lock onwer has a budget when owning a certain lock. 
// We get this value via getenv() (default is 10)
unsigned int lock_ownership_budget = 5;

///////////////////////////////////////////////////////////////////// Others
// let each thread know who he is
__thread size_t my_tid = INVALID_TID;

// This is to switch-off pthread API. 
// We must garauntee that those APIs work after Qthread's ctor 
static bool qthread_initialized = false;


// Wall-clock (total elapsed time)
Timer g_total_timer;
  
// Time consumed in waiting token
Timer g_serial_timer;

// A logfile handle
FILE * g_logfile_handle;




class Qthread {

private:
  // We use this to track thread's status. If a thread is waken up by 
  // any sync operation (e.g. cond signal/mutex release), we move its entry from sleep list
  // to active list, or vice versa.
  enum ThreadStatus {
    STATUS_READY = 0,         // Different reasons for sleeping
    STATUS_LOCK_WAITING,      // The thread is waiting for a lock
    STATUS_COND_WAITING,      // The thread is waiting for a condition to become true
    STATUS_BARR_WAITING,      // The thread is waiting for other threads to pass the barrier
    STATUS_JOINING_THREAD,    // The thread is joining other threads
    STATUS_HIBERNATE,         // The thread will not enter critical section for a long time (for some reasons)
  };


  class ThreadEntry {
  public:
    size_t tid;     // The thread we assigned
    pthread_t pid;  // Assigned by pthread_self
    int status;
    void * cond;    // each thread may wait for one cond each time   
    void * lock;
    size_t joinee_tid;  // I waiting for joining this thread

    // volatile void * barrier;
    // volatile bool broadcast; // If broadcast = true, the thread will be woken up by broadcast

    inline ThreadEntry() {}

    inline ThreadEntry(size_t tid, pthread_t pid) {
      this->tid = tid;
      this->pid = pid;
      this->status = 0;  // 0 = ready
      this->cond = NULL;
      this->lock = NULL;
      this->joinee_tid = 0;
      // this->barrier = NULL;
    }

    inline ThreadEntry(const ThreadEntry & entry) {
      this->tid = entry.tid;
      this->pid = entry.pid;
      this->status = entry.status;
      this->cond = entry.cond;
      this->lock = entry.lock;
      this->joinee_tid = entry.joinee_tid;
      // this->barrier = entry.barrier;
    }
  };

  // We will construct the instance in ctor
  static Qthread instance;

  // Maintain all the active thread entries in a vector
  // All threads in this list is participating the token passing game
  std::vector<ThreadEntry> _active_entries;

  // We will hold the hibernated threads in another vector
  std::vector<ThreadEntry> _sleep_entries;

  // An increasing number used to assign unique thread id
  size_t _thread_unique_id;

  // Pointing to the thread entry who holds the toke 
  // NOTE: This can be -1 (no one has token). When equal -1, the value
  // in _token_tid is invalid. So each time we turn _token_pos to 0, we must update _token_tid
  volatile int _token_pos;

  // The thread id of the token owner
  volatile size_t _token_tid;

  // Please see the definition of CondWait()
  pthread_mutex_t _cond_mutex;

  // Next random thread gets token 
  bool _random_next;



public:


  static inline Qthread& GetInstance(void) { return instance; }


  //////////////////////////////////////////////////////////////////////////////////// Constructor  
  // Intially we register the main process (#0) in the activelist, and set up its tid
  // #0 is always registered the activelist for the whole lifetime of the program
  // #0 will get the token first, and the token will never be called back
  Qthread(bool random_next = false):
  _active_entries(),
  _sleep_entries()
  {

    // We must call this function, before we can use any pthread references
    init_pthread_reference();

    // Prepare the log file
#ifdef DEBUG_QTHREAD
    g_logfile_handle = fopen("qthread.log", "w");
    assert(g_logfile_handle != NULL);
#endif


    // We allocate memory before hand
    _active_entries.reserve(1024);
    _sleep_entries.reserve(1024);

    // We register main thread manually (all the other threads are registerd when being spawned)
    my_tid = MAIN_TID;    
    DEBUG_QTHREAD("# Regsiter(%lu): %lu\n", my_tid, my_tid);
    DEBUG_QTHREAD("# InitToken(%lu): %lu\n", my_tid, my_tid);    
    ThreadEntry entry(MAIN_TID, 65536);         // Note: we do not care its pthread_id 
    entry.status = STATUS_READY;
    _active_entries.push_back(entry);
    _token_pos = 0;
    _token_tid = MAIN_TID;
    _thread_unique_id = MAIN_TID + 1;      // Unique Id is the next number of MAIN_TID
  
    // whether we use random_next strategy
    _random_next = random_next; 


    // We get the lock budget from the environment
    {
      char * str = getenv("QTHREAD_LOCK_BUDGET");
      if (str) {
        lock_ownership_budget = atoi(str);
      } 
    }

    // Set up the random number generator
    settable(12345,65435,34221,12345,9983651,95746118);

    ppthread_mutex_init(&g_spawn_lock, NULL);
    ppthread_mutex_init(&_cond_mutex, NULL);


#ifdef PROFILE_QTHREAD
    g_total_timer.Start();
#endif

    qthread_initialized = true;

  }


  ~Qthread() {

    qthread_initialized = false;

    ppthread_mutex_destroy(&_cond_mutex);
    ppthread_mutex_destroy(&g_spawn_lock);

#ifdef DEBUG_QTHREAD
    fclose(g_logfile_handle);
#endif


#ifdef PROFILE_QTHREAD
    g_total_timer.Pause();
    printf("Total Time: %ld\n", g_total_timer.Total());
    //printf("Serial Time: %ld @ %ld\n", _stat_serial.Total(), _stat_serial.Times()); 
#endif

  }


  //////////////////////////////////////////////////////////////////////////// Qthread Basics
  // Do nothing but make token can be passed through
  void DummySync(void) {
    

#if TOKEN_OWNERSHIP_ON 
    // Ownership give up
    // It is not a good idea if a thread holds the ownership too long
    // even if it stops acquiring its owned lock
    // In this case we must force it to give up the ownership   
    if (owned_spinner != NULL) {
      DEBUG_QTHREAD("# LoseSpinLock(%p): %lu\n", owned_spinner, my_tid);
      // If the lock is logically locked, then we can not release the lock
      if (owned_spinner_is_locked == false)
        ppthread_spin_unlock((pthread_spinlock_t *) owned_spinner);
      owned_spinner = NULL;
      owned_spinner_budget = 0;
    }

    if (owned_mutex != NULL) {
      DEBUG_QTHREAD("# LoseMutexLock(%p): %lu\n", owned_mutex, my_tid);
      // We just act as what we pretend to be (if we pretend to be unlocked, then unlock it)     
      if (owned_mutex_is_locked == false)
        ppthread_mutex_unlock((pthread_mutex_t *) owned_mutex);
      owned_mutex = NULL;
      owned_mutex_budget = 0;
    }

#endif    

    waitToken();
    DEBUG_QTHREAD("# DumSync: %lu\n", my_tid);
    passToken();
    return;
  }

  //The followin API is for activat/deactivate thread externally
  int HibernateThread(size_t tid) {
    waitToken();
    //DEBUG_QTHREAD("# Hibernate(%lu): %lu\n", tid, my_tid);
    int retval = deactivateThread(tid, STATUS_HIBERNATE);
    passToken();
    return retval;
  }

  int WakeUpThread(size_t tid) {
    waitToken();
    //("# WakeUp(%lu): %lu\n", tid, my_tid);
    int retval = activateThread(tid);
    passToken();
    return retval;
  }

  // A wrapper for calling through pthread id
  int HibernateThreadByPid(pthread_t pid) {
    waitToken();
    ThreadEntry * entry = getActiveEntryByPid(pid);
    if (entry == NULL) {
      DEBUG_QTHREAD("# Hibernate404: %lu\n", my_tid);
      return 1;
    }
    size_t tid = entry->tid;
    //DEBUG_QTHREAD("# Hibernate(%lu): %lu\n", tid, my_tid);
    int retval = deactivateThread(tid, STATUS_HIBERNATE);
    passToken();
    return retval;
  }
 
  int WakeUpThreadByPid(pthread_t pid) {
    waitToken();
    ThreadEntry * entry = getActiveEntryByPid(pid);
    if (entry == NULL) { 
      DEBUG_QTHREAD("# WakeUp404: %lu\n", my_tid);
      return 1;
    }
    size_t tid = entry->tid;
    //DEBUG_QTHREAD("# WakeUp(%lu): %lu\n", tid, my_tid);
    int retval = activateThread(tid);
    passToken();
    return retval;
  }


  //////////////////////////////////////////////////////////////////////////// Pthread Basics
  // Spawing is treated as a sync point as well
  int Spawn(pthread_t * pid, const pthread_attr_t * attr, ThreadFunction func, void * arg) {
    
    waitToken();

    // This lock is just for the safety of 'thread_param' (global variable).
    // NOTE: this lock will make the initialization of threads be *serialized*. When we have a lot of 
    // threads to spawn, that may hurt performance. Try to replace it with a better mechanism later.
    ppthread_mutex_lock(&g_spawn_lock);

    // Call gcc atomic operation to assign an increasing unique number
    size_t tid = __sync_fetch_and_add(&_thread_unique_id, 1);

    // Hook up the thread function and arguments
    g_thread_param.func = func;
    g_thread_param.arg = arg;
    g_thread_param.tid = tid;

    // The unlock of spawn_lock is located in the ThreadFuncWrapper we passed to ppthread_create()
    int retval = ppthread_create(pid, attr, fake_thread_entry, &g_thread_param);
    DEBUG_QTHREAD("# Spawn(%lu+%lu): %lu\n", *pid, tid, my_tid);

    
#if 1
    waitToken();
#endif


    // FIXME: remove it later
    // Check duplication in active_entries
    for (unsigned int i = 0; i < _active_entries.size(); i++) {
      if (_active_entries[i].tid == tid) {
        DEBUG_QTHREAD("RegDuplicate(%lu): %lu\n", tid, my_tid);
        assert(0);
      }
    }

    // Register after we have created the thread. 
    DEBUG_QTHREAD("# Regsiter(%lu): %lu\n", tid, my_tid);
    ThreadEntry entry(tid, *pid);  // We also record the pthread_id
    entry.status = STATUS_READY;
    _active_entries.push_back(entry);

    passToken();
    return retval;
  }



  // Call before a thread quit
  void Terminate(void) {


    DEBUG_QTHREAD("# Terminate: %lu\n", my_tid);


#if TOKEN_OWNERSHIP_ON
    // Ownership give up
    // It is not a good idea if a thread holds the ownership too long
    // even if it stops acquiring its owned lock
    // In this case we must force it to give up the ownership   
    if (owned_spinner != NULL) {
      DEBUG_QTHREAD("# LoseSpinLock(%p): %lu\n", owned_spinner, my_tid);
      // If the lock is logically locked, then we can not release the lock
      if (owned_spinner_is_locked == false)
        ppthread_spin_unlock((pthread_spinlock_t *) owned_spinner);
      owned_spinner = NULL;
      owned_spinner_budget = 0;
    }

    if (owned_mutex != NULL) {
      DEBUG_QTHREAD("# LoseMutexLock(%p): %lu\n", owned_mutex, my_tid);
      // We just act as what we pretend to be (if we pretend to be unlocked, then unlock it)     
      if (owned_mutex_is_locked == false)
        ppthread_mutex_unlock((pthread_mutex_t *) owned_mutex);
      owned_mutex = NULL;
      owned_mutex_budget = 0;
    }

#endif    

    waitToken();

    // It is my duty to wake up my joiner .But it is possible that I can not find the one
    // who joins me. This happens when the joinee executes faster than the joiner.    
    for (unsigned int i = 0; i < _sleep_entries.size(); i++) {
      ThreadEntry entry = _sleep_entries[i];
      // My joiner record my pid joiner. Just scan the sleep entry and find who is joining me
      if (entry.joinee_tid == my_tid) {
        DEBUG_QTHREAD("# JoinActivate(%lu): %lu\n", entry.tid, my_tid);
        entry.status = STATUS_READY;   // set ready
        entry.joinee_tid = 0;
        _active_entries.push_back(entry);  
        _sleep_entries.erase(_sleep_entries.begin() + i);
        break;   
      }
    }
    
    // Locate the my thread entry and delete
    bool isFound = false;
    for (unsigned int i = 0; i < _active_entries.size(); i++) {
      if (_active_entries[i].tid == my_tid) {
        isFound = true;
        DEBUG_QTHREAD("# DeregisterMe: %lu\n", my_tid);
        _active_entries.erase(_active_entries.begin() + i);
        // NOTE: here we just adjust token pos back and do not change _token_tid
        if (_active_entries.empty()) {
          _token_pos = -1;
          DEBUG_QTHREAD("# CallbackToken: %lu\n", my_tid);
        } else { 
          _token_pos = (_token_pos - 1 + _active_entries.size()) % _active_entries.size();  // Roll back the pos
        }
      }
    }
    assert(isFound == true);

    // // Pass token to the thread that should be woken up
    // _token_pos = _active_entries.size() - 1;
    // _token_tid = _active_entries[_token_pos].tid;
    // DEBUG_QTHREAD("# throwToken(%lu): %lu\n", _token_tid, my_tid);

    passToken();


    // We clear my_tid when terminating
    my_tid = INVALID_TID;  

    return;
  }



  // pthread_join will suspand until the joinee terminates, which will affect the token passing game
  // So we must let him sleep and leave token passing game
  int Join(pthread_t pid, void ** val) {


#if TOKEN_OWNERSHIP_ON 
    // Ownership give up
    // It is not a good idea if a thread holds the ownership too long
    // even if it stops acquiring its owned lock
    // In this case we must force it to give up the ownership   
    if (owned_spinner != NULL) {
      DEBUG_QTHREAD("# LoseSpinLock(%p): %lu\n", owned_spinner, my_tid);
      // If the lock is logically locked, then we can not release the lock
      if (owned_spinner_is_locked == false)
        ppthread_spin_unlock((pthread_spinlock_t *) owned_spinner);
      owned_spinner = NULL;
      owned_spinner_budget = 0;
    }

    if (owned_mutex != NULL) {
      DEBUG_QTHREAD("# LoseMutexLock(%p): %lu\n", owned_mutex, my_tid);
      // We just act as what we pretend to be (if we pretend to be unlocked, then unlock it)     
      if (owned_mutex_is_locked == false)
        ppthread_mutex_unlock((pthread_mutex_t *) owned_mutex);
      owned_mutex = NULL;
      owned_mutex_budget = 0;
    }
#endif    

    int retval = -1;


    DEBUG_QTHREAD("# Join(%lu): %lu\n", pid, my_tid);


    
    waitToken();

    size_t joinee_tid;


    bool isFound = false;

    // FIXME: this conversion is time-comsuming because we need to scan both lists
    // Conver joinee's pid to tid
    for (unsigned int i = 0; i < _active_entries.size(); i++) {
      ThreadEntry entry = _active_entries[i];
      if (entry.pid == pid) {
        isFound = true;
        joinee_tid = entry.tid;
        break;
      }
    }

    // The thread I want to join may be found in the sleep list
    if (!isFound) {
      for (unsigned int i = 0; i < _sleep_entries.size(); i++) {
        ThreadEntry entry = _sleep_entries[i];
        if (entry.pid == pid) {
          isFound = true;
          joinee_tid = entry.tid;
          break;
        }
      }
    }

    // It is possible that we can not find the entry in both lists
    // This happens when the joinee executes faster than the joiner.
    if (!isFound) {
      //DEBUG_QTHREAD("# Join404: %lu\n", my_tid);
      retval = ppthread_join(pid, val);
      passToken();            
      return retval;
    }


    // If the joinee is still in the stage, I have to sleep
    // Move my thread entry into sleep list
    isFound = false;
    for (unsigned int i = 0; i < _active_entries.size(); i++) {
      ThreadEntry entry = _active_entries[i];  // Deep copy
      // locate myself in the active entries
      if (entry.tid == my_tid) {
        isFound = true;
        DEBUG_QTHREAD("# JoinDeact(%lu): %lu\n", joinee_tid, my_tid);
        entry.status = STATUS_JOINING_THREAD;     // Join thread
        entry.joinee_tid = joinee_tid;            // I fall asleep because I want to join this thread

        // Here we can change the order of being woken up
#if FIRST_SLEEP_FIRST_WOKENUP
        _sleep_entries.push_back(entry);
#else
        _sleep_entries.insert(_sleep_entries.begin(), entry); // append at head
#endif
        _active_entries.erase(_active_entries.begin() + i);
        // NOTE: here we just adjust token pos back and do not change _token_tid
        if (_active_entries.empty()) {
          _token_pos = -1;
          DEBUG_QTHREAD("# CallbackToken: %lu\n", my_tid);
        } else { 
          _token_pos = (_token_pos - 1 + _active_entries.size()) % _active_entries.size();  // Roll back the pos
        }        
        break;
      }
    }

    assert(isFound == true);
    passToken();


    // Start to join that thread (suspand until the thread finishes)
    retval = ppthread_join(pid, val);

    // sleep and wake up here
    //DEBUG_QTHREAD("# JoinOK(%lu): %lu\n", joinee_tid, my_tid);

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

  int MutexDestroy(pthread_mutex_t * mutex) {


#if TOKEN_OWNERSHIP_ON 
    // Ownership give up    
    // If I am destroying my owned lock, I will give the ownership as well
    // If we should not delay this behavior after we have freed this lock, 
    // (e.g. in dummy_sync or before terminate). otherwise, the ppthread_mutex_unlock() might cause problem 
    if (owned_mutex == (void *) mutex) {
      DEBUG_QTHREAD("# LoseMutexLock(%p): %lu\n", owned_mutex, my_tid);
      // We just act as what we pretend to be (if we pretend to be unlocked, then unlock it)     
      if (owned_mutex_is_locked == false)
        ppthread_mutex_unlock((pthread_mutex_t *) owned_mutex);
      owned_mutex = NULL;
      owned_mutex_budget = 0;
    }
#endif  

    return ppthread_mutex_destroy(mutex);
  }


  // In this version, if a thread want to acquire a lock, it will possibly leave the token passing game 
  // The determinsim is ensured by enqueueing the thread into the sleeping list in a deterministic order
  // No ownership !
  int LockAcquire(pthread_mutex_t * lock) {

    if (my_tid == INVALID_TID)
      return ppthread_mutex_lock(lock);


#if TOKEN_OWNERSHIP_ON 
    // Ownership give up
    // It is not a good idea if a thread holds the ownership too long
    // even if it stops acquiring its owned lock
    // In this case we must force it to give up the ownership   
    if (owned_spinner != NULL) {
      DEBUG_QTHREAD("# LoseSpinLock(%p): %lu\n", owned_spinner, my_tid);
      // If the lock is logically locked, then we can not release the lock
      if (owned_spinner_is_locked == false)
        ppthread_spin_unlock((pthread_spinlock_t *) owned_spinner);
      owned_spinner = NULL;
      owned_spinner_budget = 0;
    }

    if (owned_mutex != NULL) {
      DEBUG_QTHREAD("# LoseMutexLock(%p): %lu\n", owned_mutex, my_tid);
      // We just act as what we pretend to be (if we pretend to be unlocked, then unlock it)     
      if (owned_mutex_is_locked == false)
        ppthread_mutex_unlock((pthread_mutex_t *) owned_mutex);
      owned_mutex = NULL;
      owned_mutex_budget = 0;
    }

#endif    



    int retval = -1;

    //DEBUG_QTHREAD("# LockAcq...: %lu\n", my_tid);

    /////////////////////////////////////////////////////////////////////////////////////////
    //               Phase I: Trylock & Sleep Phase 
    // In this phase, we attempt to acquire that lock for the first time. If we fail to acquire
    // the lock, then we enter sleep state.
    waitToken();

    // Trylock attemption: sleep only if I can not acquire lock currently
    retval = ppthread_mutex_trylock(lock);
    // NOTE: Here we consider retval is either EBUSY or OK 
    if (retval != EBUSY) {
      // Acquired the lock
      DEBUG_QTHREAD("# LockAcq0(%p): %lu\n", lock, my_tid);
      passToken();
      return retval;
    }

    // Force the thread to sleep and leave token passing game
    bool isFound = false;
    for (unsigned int i = 0; i < _active_entries.size(); i++) {
      ThreadEntry entry = _active_entries[i];  // Deep copy
      // locate myself in the active entries
      if (entry.tid == my_tid) {
        isFound = true;
        DEBUG_QTHREAD("# LockDeact(%p): %lu\n", lock, my_tid);
        entry.status = STATUS_LOCK_WAITING;     // mutex wait
        entry.lock = (void *) lock;

        // Here we can change the order of being woken up
#if FIRST_SLEEP_FIRST_WOKENUP
        _sleep_entries.push_back(entry);
#else
        _sleep_entries.insert(_sleep_entries.begin(), entry); // append at head
#endif

        _active_entries.erase(_active_entries.begin() + i);
        // NOTE: here we just adjust token pos back and do not change _token_tid
        if (_active_entries.empty()) {
          _token_pos = -1;
          DEBUG_QTHREAD("# CallbackToken: %lu\n", my_tid);
        } else { 
          _token_pos = (_token_pos - 1 + _active_entries.size()) % _active_entries.size();  // Roll back the pos
        }        
        break;
      }
    }
    assert(isFound == true);

    passToken();

    /////////////////////////////////////////////////////////////////////////////////////////
    //               Phase II: Wakeup & Acquire Phase
    // I may be woken up at anytime when other thread call LockRelease(), since the OS can not
    // guarantee the FIFO order. So we hope the caller of LockRelease() can wake me up and pass
    // the token to me. Until he passes the token to me, I will continue to sleep on that lock. 
    while (true) {

      // Acquired lock anyway before we check activity
      // DEBUG_QTHREAD("# LockSleep(%p): %lu\n", lock, my_tid);
      retval = ppthread_mutex_lock(lock);
      // DEBUG_QTHREAD("# LockWakeup(%p): %lu\n", lock, my_tid);

      // IMPORTANT: Check whether I have token instead of scaning active entries
      // since LockRelease() will pass the token to me
      if (my_tid != _token_tid) {
        // DEBUG_QTHREAD("# LockYield(%p): %lu\n", lock, my_tid);
        ppthread_mutex_unlock(lock);

      } else { 
        // If we arrive here, this thread must have *really* acquired the lock
        DEBUG_QTHREAD("# LockAcq(%p): %lu\n", lock, my_tid);  
        break;
      }
    }
    
    passToken();

    return retval;
  }





  // Wake up the first thread (who is wait the lock )in the sleep entries 
  int LockRelease(pthread_mutex_t * lock) {

    if (my_tid == INVALID_TID) 
      return ppthread_mutex_unlock(lock);

    int retval = -1;
    //DEBUG_QTHREAD("# ReleaseLock(%p): %lu\n", lock, my_tid);

#if TOKEN_OWNERSHIP_ON
    // Phase I: Owned-lock release
    if (owned_mutex == (void *) lock) {

      // We don't really need to call unlock(), because we still owned that lock
      DEBUG_QTHREAD("# OwnMutexLockRel(%p)[%u]: %lu\n", lock, owned_mutex_budget, my_tid);

      // Pretend to unlock
      assert(owned_mutex_is_locked == true);
      owned_mutex_is_locked = false;

      // To avoid starvation, the lock owner has a pre-defined budget. 
      // If the budget has been used out, then thread has to yield the ownership. 
      owned_mutex_budget--;

      // The only condition to yield a lock is because we have used out its budget
      if (owned_mutex_budget == 0) {
        DEBUG_QTHREAD("# LoseMutexLock(%p): %lu\n", owned_mutex, my_tid);
        owned_mutex = NULL;
        // 1. If we give up ownership in LockRelease(), it is always safe to call 
        // ppthread_mutex_unlock(), since the lock must be logically and (physically) locked 

        // 2. Here we just give up the ownership but haven't release the lock
        // Because during the ownership, other thread that call LockAcquire() will
        // sleep on this lock, so it's my duty to wake up them 
      } else { 
        return 0;
      }
    }
#endif


    waitToken();

    DEBUG_QTHREAD("# LockRel(%p): %lu\n", lock, my_tid);

    // The search will possibly fail if there's no other thread sleeping on that lock
    // In this case, we don't have to wakeup anyone, just release the lock
    bool isFound = false;
    for (unsigned int i = 0; i < _sleep_entries.size(); i++) {
      ThreadEntry entry = _sleep_entries[i];
      if (entry.lock == (void *) lock) {
        isFound = true;
        DEBUG_QTHREAD("# LockAct(%lu): %lu\n", entry.tid, my_tid);
        entry.status = STATUS_READY;   // set ready
        entry.lock = NULL;
        _active_entries.push_back(entry);  
        _sleep_entries.erase(_sleep_entries.begin() + i);
        break;   
      }
    }

    // Wake up other threads who is waiting on this lock
    // retval = ppthread_mutex_unlock(lock); 

    // If some thread is blocked for this lock, we just pass token to him
    if (isFound) {

      // This operation may wake up other thread than the target one
      // but in our approach, they will sleep again
      retval = ppthread_mutex_unlock(lock); 

      // Pass token to the thread that should be woken up
      _token_pos = _active_entries.size() - 1;
      _token_tid = _active_entries[_token_pos].tid;
      DEBUG_QTHREAD("# throwToken(%lu): %lu\n", _token_tid, my_tid);


    } else { // This path is followed when there's no thread sleeping on that lock
      retval = ppthread_mutex_unlock(lock); 
      passToken();
    }

    return retval;
  }


  int LockAcquireTry(pthread_mutex_t * mutex) {

    int retval = -1;
    //DEBUG_QTHREAD("# MutexTryLock: %lu\n", my_tid);

#if TOKEN_OWNERSHIP_ON

    // Phase I: Own lock Acquisition
    // If I own this lock, then we don't need to wait token to acquire it. 
    // Meanwhile the acquisition must succeed.
    if (owned_mutex == (void *) mutex) {
      // We don't really need to lock it since the ownership is exclusive
      // The lock is owned by me, I haven't released the lock at all
      // I can hold this lock until I release the ownership     
      if (owned_mutex_is_locked) {
        DEBUG_QTHREAD("# OwnMutexTryLockBusy(%p): %lu\n", mutex, my_tid);
        return EBUSY;
      } else {
        DEBUG_QTHREAD("# OwnMutexTryLockOK(%p): %lu\n", mutex, my_tid);
        owned_mutex_is_locked = true;
        return 0;
      }
    } 

    // Considering trylock() can be called in a while loop
    // To avoid starvation, each time we pass here, we comsume the budget of owned locks

    // if I own another lock, I have to consume its budget as well
    // and give up the ownership if necessary
    if (owned_mutex != NULL && owned_mutex != (void *) mutex) { 
      --owned_mutex_budget;
      if (owned_mutex_budget == 0) {
        DEBUG_QTHREAD("# LoseMutexLock(%p): %lu\n", owned_mutex, my_tid);
        // We just act as what we pretend to be (if we pretend to be unlocked, then unlock it)     
        if (owned_mutex_is_locked == false) {
          ppthread_mutex_unlock((pthread_mutex_t *) owned_mutex);
          owned_mutex = NULL;
        }
      }
    }

    // Consume the budget of the owned spinner, and give up the ownership if should
    if (owned_spinner != NULL) {
      --owned_spinner_budget;
      if (owned_spinner_budget == 0) {
        DEBUG_QTHREAD("# LoseSpinLock(%p): %lu\n", owned_spinner, my_tid);
        // We just act as what we pretend to be (if we pretend to be unlocked, then unlock it)     
        if (owned_spinner_is_locked == false)
          ppthread_spin_unlock((pthread_spinlock_t *) owned_spinner);
          owned_spinner = NULL;        
      }
    }

#endif



    waitToken();          
    retval = ppthread_mutex_trylock(mutex);

    // This is a trylock, so we do not desire for locks
    // If trylock failed, just return directly
    if (retval == EBUSY) {
      DEBUG_QTHREAD("# MutexTryLockBusy: %lu\n", my_tid);
      passToken();
      return EBUSY;  
    } 


    // If we arrive here, this thread must have acquired the lock
    DEBUG_QTHREAD("# MutexTryLockOK: %lu\n", my_tid);



#if TOKEN_OWNERSHIP_ON
    // Phase III: Claim the ownership 
    // When I encounter a new lock, but I have already owned other lock, I have to 
    // yield the old lock, since each thread can only own one lock each time
    // So that the old lock can be acquired by other threads
    if (owned_mutex != NULL) {
      DEBUG_QTHREAD("# LoseMutexLock(%p): %lu\n", owned_mutex, my_tid);
      // We just act as what we pretend to be (if we pretend to be unlocked, then unlock it)     
      if (owned_mutex_is_locked == false)
        ppthread_mutex_unlock((pthread_mutex_t *) owned_mutex);
      owned_mutex = NULL;
      owned_mutex_budget = 0;
    } 
    
    // We always treat an encountered lock as a distinct lock, and take the ownership of it.     
    // NOTE: We take the ownership in the serial phase, so the determinism is guaranteed
    owned_mutex = (void * ) mutex;
    owned_mutex_budget = lock_ownership_budget;  // recharge the budget
    owned_mutex_is_locked = true;            // Intially, the owned lock is also locked

    DEBUG_QTHREAD("# OwnMutexLock(%p): %lu\n", mutex, my_tid);
#endif


    passToken();
    return retval;
  }


  // Abandoned API
  // This is naive version of mutex_lock(). Thread busy-waits for its turn
  int MutexLock(pthread_mutex_t * mutex) {

    int retval = -1;

    //DEBUG_QTHREAD("# MutexLock: %lu\n", my_tid);

    while (true) { // As long as retval==EBUSY, the loop will go on
      waitToken();
      retval = ppthread_mutex_trylock(mutex);
      // Any thread having token has the right to acquire the lock
      if (retval == EBUSY) {
        // If thread fails to acquire the lock, then pass the token immediately
        // This prevent the case that thread sleep on the mutex while holding token
        DEBUG_QTHREAD("# MutexLockBusy: %lu\n", my_tid);
        passToken();
      } else {
        break;
      }
    }

    // If we arrive here, this thread must have acquired the lock
    DEBUG_QTHREAD("# MutexLockAcq(%p): %lu\n", mutex, my_tid); // acquire
    passToken();
    return retval;
  }



  // Abandoned API
  int MutexUnlock(pthread_mutex_t * mutex) {

    int retval = -1;
    //DEBUG_QTHREAD("# MutexUnlock: %lu\n", my_tid);
    waitToken();
    retval = ppthread_mutex_unlock(mutex);
    DEBUG_QTHREAD("# MutexLockRel(%p): %lu\n", mutex, my_tid);
    passToken();
    return retval;
  }



  // Abandoned API
  // In this version, each thread acquires the lock anyway before
  // checking the token. If it doesn't own token just yield it 
  // and repeat to acuquire the lock and then check the token...
  int MutexWaitLock(pthread_mutex_t * mutex) {

    int retval = -1;

    //DEBUG_QTHREAD("# MutexWaitLock: %lu\n", my_tid);
    while (true) {
      retval = ppthread_mutex_lock(mutex);
      // Any thread may get the lock
      DEBUG_QTHREAD("# MutexLockAcq: %lu\n", my_tid);
      // Check whether I have the token
      if (my_tid != _token_tid) {
        // if I have no token, yield the lock
        // DEBUG_QTHREAD("# MutexLockYield: %lu\n", my_tid);
        ppthread_mutex_unlock(mutex);
      } else {
        break;
      }
    }
    // Lock acquired here. Pass token and continues its execution
    passToken(); 
    return retval;
  }


  //////////////////////////////////////////////////////////////////////////////// Spinlock
  int SpinInit(pthread_spinlock_t * spinner, int shared) {
    return ppthread_spin_init(spinner, shared);
  }


  int SpinDestroy(pthread_spinlock_t * spinner) {

#if TOKEN_OWNERSHIP_ON 
    // If I am destroying my owned lock, I will give the ownership as well
    // If we should not delay this behavior after we have freed this lock, 
    // (e.g. in dummy_sync or before terminate). otherwise, the ppthread_mutex_unlock() might cause problem 
    if (owned_spinner == (void *) spinner) {
      DEBUG_QTHREAD("# LoseSpinLock(%p): %lu\n", owned_spinner, my_tid);
      // We just act as what we pretend to be (if we pretend to be unlocked, then unlock it)     
      if (owned_spinner_is_locked == false)
        ppthread_spin_unlock((pthread_spinlock_t *) owned_spinner);
      owned_spinner_is_locked = false;
      owned_spinner = NULL;
      owned_spinner_budget = 0;
    }
#endif  

    return ppthread_spin_destroy(spinner);
  }


  int SpinLock(pthread_spinlock_t * spinner) {

 
    int retval = -1;
    // DEBUG_QTHREAD("# SpinLock: %lu\n", my_tid);


#if TOKEN_OWNERSHIP_ON

    // if I own another lock, I have to consume its budget as well
    // and give up the ownership if necessary
    // Consume the budget of the owned spinner, and give up the ownership if should
    if (owned_spinner != NULL && owned_spinner != (void *) spinner) {
      --owned_spinner_budget;
      if (owned_spinner_budget == 0) {
        DEBUG_QTHREAD("# LoseSpinLock(%p): %lu\n", owned_spinner, my_tid);
        // We just act as what we pretend to be (if we pretend to be unlocked, then unlock it)     
        if (owned_spinner_is_locked == false)
          ppthread_spin_unlock((pthread_spinlock_t *) owned_spinner);
        owned_spinner = NULL;        
      }
    }

    // Consume the budget of the owned mutex, and give up the ownership if should
    if (owned_mutex != NULL) {
      --owned_mutex_budget;
      if (owned_mutex_budget == 0) {
        DEBUG_QTHREAD("# LoseMutexLock(%p): %lu\n", owned_mutex, my_tid);
        // We just act as what we pretend to be (if we pretend to be unlocked, then unlock it)     
        if (owned_mutex_is_locked == false)
          ppthread_mutex_unlock((pthread_mutex_t *) owned_mutex);
        owned_mutex = NULL;       
      }  
    }


    // Phase I: Own lock Acquisition
    // If I own this lock, then we don't need to wait token to acquire it. 
    // Meanwhile the acquisition must succeed.
    if (owned_spinner == (void *) spinner) {
      DEBUG_QTHREAD("# OwnSpinLockAcq(%p): %lu\n", spinner, my_tid);
      // We don't really need to lock it since the ownership is exclusive
      // The lock is owned by me, I don't release the lock at all
      // I can hold this lock until I release the ownership 

      assert(owned_spinner_is_locked == false);
      owned_spinner_is_locked = true;
      return 0;
      
    } 

#endif

    // Phase II: Ordinary Acquistion.
    //  If we enter this phase, we need token to progress
    // Try until we get the lock
    while (true) {
      waitToken();
      retval = ppthread_spin_trylock(spinner);
      // Any thread having token has the right to acquire the lock
      if (retval == EBUSY) {
        // If thread fails to acquire the lock, then pass the token immediately
        DEBUG_QTHREAD("# SpinLockFail: %lu\n", my_tid);
        passToken();
      } else {
        break;
      }
    }

    // If we arrive here, this thread must have acquired the lock
    DEBUG_QTHREAD("# SpinLockAcq: %lu\n", my_tid);


#if TOKEN_OWNERSHIP_ON

    // Phase III: Claim the ownership 
    // When I encounter a new lock, but I have already owned other lock, I have to 
    // yield the old lock, since each thread can only own one lock each time
    // So that the old lock can be acquired by other threads
    if (owned_spinner != NULL) {
      DEBUG_QTHREAD("# LoseSpinLock(%p): %lu\n", owned_spinner, my_tid);
      // We just act as what we pretend to be (if we pretend to be unlocked, then unlock it)     
      if (owned_spinner_is_locked == false)
        ppthread_spin_unlock((pthread_spinlock_t *) owned_spinner);
      owned_spinner = NULL;
      owned_spinner_budget = 0;
    } 
    
    // We always treat an encountered lock as a distinct lock, and take the ownership of it.     
    // NOTE: We take the ownership in the serial phase, so the determinism is guaranteed
    owned_spinner = (void * ) spinner;
    owned_spinner_budget = lock_ownership_budget;  // recharge the budget
    owned_spinner_is_locked = true;            // Initally, the owned spinner is also locked
    DEBUG_QTHREAD("# OwnSpinLock(%p): %lu\n", spinner, my_tid);
    
#endif

    passToken();
    return retval;
  }



  int SpinUnlock(pthread_spinlock_t * spinner) {

    int retval = -1;
    //DEBUG_QTHREAD("# SpinUnlock: %lu\n", my_tid);

#if TOKEN_OWNERSHIP_ON
    // Phase I: Owned-lock release
    if (owned_spinner == (void *) spinner) {

      // We don't really need to call unlock(), because we still owned that lock
      DEBUG_QTHREAD("# OwnSpinLockRel(%p)[%u]: %lu\n", spinner, owned_spinner_budget, my_tid);

      assert(owned_spinner_is_locked == true);
      // We pretend to lock that spinner, but behind the scene we don't 
      // really need to call pthread_spin_unlock()
      owned_spinner_is_locked = false;

      // To avoid starvation, the lock owner has a pre-defined budget. We cost the budget when unlocking       
      // If the budget has been used out, then thread has to yield the ownership. 
      owned_spinner_budget--;

      // The only condition to yield a lock is because we have used out its budget
      if (owned_spinner_budget == 0) {
        DEBUG_QTHREAD("# LoseSpinLock(%p): %lu\n", owned_spinner, my_tid);
        // 1. If we give up ownership in SpinUnlock(), it is always safe to call 
        // ppthread_spin_unlock(), 
        ppthread_spin_unlock(spinner);
        owned_spinner = NULL;
      } 

      return 0;
    }

#endif

    // Phase II: Ordinary Release 
    waitToken();
    retval = ppthread_spin_unlock(spinner);
    DEBUG_QTHREAD("# SpinLockRel: %lu\n", my_tid);
    passToken();
    return retval;
  }



  int SpinTrylock(pthread_spinlock_t * spinner) {

    int retval = -1;
    //DEBUG_QTHREAD("# SpinTryLock: %lu\n", my_tid);
    
#if TOKEN_OWNERSHIP_ON

    // Consume the budget of the owned mutex, and give up the ownership if should
    if (owned_mutex != NULL) {
      --owned_mutex_budget;
      if (owned_mutex_budget == 0) {
        DEBUG_QTHREAD("# LoseMutexLock(%p): %lu\n", owned_mutex, my_tid);
        // We just act as what we pretend to be (if we pretend to be unlocked, then unlock it)     
        if (owned_mutex_is_locked == false)
          ppthread_mutex_unlock((pthread_mutex_t *) owned_mutex);
        owned_mutex = NULL;       
      }  
    }

    // if I own another lock, I have to consume its budget as well
    // and give up the ownership if necessary
    // Consume the budget of the owned spinner, and give up the ownership if should
    if (owned_spinner != NULL && owned_spinner != (void *) spinner) {
      --owned_spinner_budget;
      if (owned_spinner_budget == 0) {
        DEBUG_QTHREAD("# LoseSpinLock(%p): %lu\n", owned_spinner, my_tid);
        // We just act as what we pretend to be (if we pretend to be unlocked, then unlock it)     
        if (owned_spinner_is_locked == false)
          ppthread_spin_unlock((pthread_spinlock_t *) owned_spinner);
          owned_spinner = NULL;        
      }
    }
    // Phase I: Own lock Acquisition
    // If I own this lock, then we don't need to wait token to acquire it. 
    // Meanwhile the acquisition must succeed.
    if (owned_spinner == (void *) spinner) {
      // We don't really need to lock it since the ownership is exclusive
      // The lock is owned by me, I haven't released the lock at all
      // I can hold this lock until I release the ownership  
      if (owned_spinner_is_locked) {
        DEBUG_QTHREAD("# OwnSpinLockBusy(%p): %lu\n", spinner, my_tid);        
        return EBUSY;
      } else {
        DEBUG_QTHREAD("# OwnSpinLockOK(%p): %lu\n", spinner, my_tid);
        owned_spinner_is_locked = true;
        return 0;
      }
    } 
#endif

    waitToken();
    retval = ppthread_spin_trylock(spinner);
      
    // if trylock failed, we no longer check token, since we do not lock 
    if (retval == EBUSY) {
      DEBUG_QTHREAD("# SpinTryLockBusy: %lu\n", my_tid);
      passToken();
      return EBUSY;
    }   
    
    // If we arrive here, this thread must have acquired the lock
    DEBUG_QTHREAD("# SpinTryLockOK: %lu\n", my_tid);

#if TOKEN_OWNERSHIP_ON
    // Phase III: Claim the ownership 
    // When I encounter a new lock, but I have already owned other lock, I have to 
    // yield the old lock, since each thread can only own one lock each time
    // So that the old lock can be acquired by other threads
    if (owned_spinner != NULL) {
      DEBUG_QTHREAD("# LoseSpinLock(%p): %lu\n", owned_spinner, my_tid);
      // We just act as what we pretend to be (if we pretend to be unlocked, then unlock it)     
      if (owned_spinner_is_locked == false)
        ppthread_spin_unlock((pthread_spinlock_t *) owned_spinner);
      owned_spinner = NULL;
      owned_spinner_budget = 0;
    } 
    
    // We always treat an encountered lock as a distinct lock, and take the ownership of it.     
    // NOTE: We take the ownership in the serial phase, so the determinism is guaranteed
    owned_spinner = (void * ) spinner;
    owned_spinner_budget = lock_ownership_budget;  // recharge the budget
    owned_spinner_is_locked = true;            // Initally, the owned spinner is also locked
    DEBUG_QTHREAD("# OwnSpinLock(%p): %lu\n", spinner, my_tid);
#endif


    passToken();
    return retval;
  }


  //////////////////////////////////////////////////////////////////////////////// Read/Write lock

  int RwLockInit(pthread_rwlock_t * rwlock, const pthread_rwlockattr_t * attr) {
    return ppthread_rwlock_init(rwlock, attr);
  }

  int RwLockDestroy(pthread_rwlock_t * rwlock) {
    return ppthread_rwlock_destroy(rwlock);
  }  

  int RdLock(pthread_rwlock_t * rwlock) {


    int retval = -1;
    //DEBUG_QTHREAD("# RdLock: %lu \n", my_tid);
    while (true) {
      waitToken();
      retval = ppthread_rwlock_tryrdlock(rwlock);
      // Any thread having token has the right to acquire the lock
      if (retval == EBUSY) {
        // If thread fails to acquire the lock, then pass the token immediately
        DEBUG_QTHREAD("# RdLockBusy(%p): %lu\n", rwlock, my_tid);
        passToken();
      } else {
        break;
      }
    }
    // If we arrive here, this thread must have acquired the lock
    DEBUG_QTHREAD("# RdLockAcq(%p): %lu\n", rwlock, my_tid);
    passToken();
    return retval;
  }

  int WrLock(pthread_rwlock_t * rwlock) {


    int retval = -1;
    //DEBUG_QTHREAD("# WrLock: %lu\n", my_tid);
    while (true) {
      waitToken();
      retval = ppthread_rwlock_trywrlock(rwlock);
      // Any thread having token has the right to acquire the lock
      if (retval == EBUSY) {
        // If thread fails to acquire the lock, then pass the token immediately
        DEBUG_QTHREAD("# WrLockBusy(%p): %lu\n", rwlock, my_tid);
        passToken();
      } else {
        break;
      }
    }
    
    // If we arrive here, this thread must have acquired the lock
    DEBUG_QTHREAD("# WrLockAcq(%p): %lu\n", rwlock, my_tid);
    passToken();   
    return retval;
  }


  int RwUnlock(pthread_rwlock_t * rwlock) {

    int retval = -1;
    //DEBUG_QTHREAD("# RwUnlock: %lu\n", my_tid);
    waitToken();
    retval = ppthread_rwlock_unlock(rwlock);
    DEBUG_QTHREAD("# RwLockRelease(%p): %lu\n", rwlock, my_tid);
    passToken();
    return retval;
  }


  int RdTryLock(pthread_rwlock_t * rwlock) {


    int retval = -1;
    //DEBUG_QTHREAD("# TryRdLock: %lu\n", my_tid);
    
    waitToken();
    retval = ppthread_rwlock_tryrdlock(rwlock);

    // if trylock failed, we no longer check token, since we do not lock 
    if (retval == EBUSY) {
      DEBUG_QTHREAD("# TryRdLockBusy(%p): %lu\n", rwlock, my_tid);
      passToken();
      return EBUSY;
    } 
      
    // If we arrive here, this thread must have acquired the lock
    DEBUG_QTHREAD("# TryRdLockAcq(%p): %lu\n", rwlock, my_tid);
    passToken();
    return retval;
  }

  int WrTryLock(pthread_rwlock_t * rwlock) {


    int retval = -1;
    //DEBUG_QTHREAD("# TryWrLock: %lu\n", my_tid);

    waitToken();
    retval = ppthread_rwlock_trywrlock(rwlock);
    
    // if trylock failed, we no longer check token, since we do not lock 
    if (retval == EBUSY) {
      DEBUG_QTHREAD("# TryWrLockBusy(%p): %lu\n", rwlock ,my_tid);
      passToken();
      return EBUSY;
    } 
    
    // If we arrive here, this thread must have acquired the lock
    DEBUG_QTHREAD("# TryWrLockAcq(%p): %lu\n", rwlock, my_tid);
    passToken();
    return retval;
  }  



  ///////////////////////////////////////////////////////////////////////////////// Condition Variables
  int CondInit(pthread_cond_t * cond, const pthread_condattr_t * attr) {
    return ppthread_cond_init(cond, attr);
  }

  int CondDestroy(pthread_cond_t * cond) {
    return ppthread_cond_destroy(cond);
  }



  // The condwait ressembles the implementation of lock acquire very much
  int CondWait(pthread_cond_t * cond, pthread_mutex_t * cond_mutex) {
    int retval = -1;


#if TOKEN_OWNERSHIP_ON 
    // Ownership give up
    // It is not a good idea if a thread holds the ownership too long
    // even if it stops acquiring its owned lock
    // In this case we must force it to give up the ownership   
    if (owned_spinner != NULL) {
      DEBUG_QTHREAD("# LoseSpinLock(%p): %lu\n", owned_spinner, my_tid);
      // If the lock is logically locked, then we can not release the lock
      if (owned_spinner_is_locked == false)
        ppthread_spin_unlock((pthread_spinlock_t *) owned_spinner);
      owned_spinner = NULL;
      owned_spinner_budget = 0;
    }

    if (owned_mutex != NULL) {
      DEBUG_QTHREAD("# LoseMutexLock(%p): %lu\n", owned_mutex, my_tid);
      // We just act as what we pretend to be (if we pretend to be unlocked, then unlock it)     
      if (owned_mutex_is_locked == false)
        ppthread_mutex_unlock((pthread_mutex_t *) owned_mutex);
      owned_mutex = NULL;
      owned_mutex_budget = 0;
    }

#endif    

    /////////////////////////////////////////////////////////////////////////////////////////
    //                Phase II: Sleep Phase
    // Before we sleep, move the entry to sleep list, so that the token passing game can go on
    waitToken();
    bool isFound = false;
    // Deactive myself first
    for (unsigned int i = 0; i < _active_entries.size(); i++) {
      if (_active_entries[i].tid == my_tid) {
        isFound = true;
        DEBUG_QTHREAD("# CondDeact(%p): %lu\n", cond, my_tid);
        ThreadEntry entry = _active_entries[i]; // Deep copy
        entry.status = STATUS_COND_WAITING;     // Cond wait
        entry.cond = (void *) cond;

        // Here we can change the order of being woken up
#if FIRST_SLEEP_FIRST_WOKENUP
        _sleep_entries.push_back(entry);
#else
        _sleep_entries.insert(_sleep_entries.begin(), entry); // append at head
#endif
        _active_entries.erase(_active_entries.begin() + i);
        // NOTE: here we just adjust token pos back and do not change _token_tid
        if (_active_entries.empty()) {
          _token_pos = -1;
          DEBUG_QTHREAD("# CallbackToken: %lu\n", my_tid);
        } else { 
          _token_pos = (_token_pos - 1 + _active_entries.size()) % _active_entries.size();  // Roll back the pos
        }
        break;
      }
    }
    assert(isFound == true);



    // IMPORTANT: Since the thread has entered sleep list (out of token passing game), 
    // We yield the cond lock, so that another thread can acquire this lock and check cond and fall asleep
    DEBUG_QTHREAD("# CondLockRel(%p): %lu\n", cond_mutex, my_tid);
    ppthread_mutex_unlock(cond_mutex);
    passToken();


    /////////////////////////////////////////////////////////////////////////////////////////
    //              Phase II: Wakeup Phase
    // Sleeping until I am *really* active. Each waking thread check whether it is in active
    // list. If not, keep sleeping on that cond. 
    // This loop prevent "false wake-up" triggered by cond_signal()
    // NOTE: getActiveEntry() is not thread-safe! So we must guarantee that other threads
    // can not modify the active entries while I am checking it

    while (true) {
      // Suspend on the real cond. 
      DEBUG_QTHREAD("# CondSleep(%p): %lu\n", cond, my_tid);
      retval = ppthread_cond_wait(cond, &_cond_mutex);
      DEBUG_QTHREAD("# CondWakeup(%p): %lu\n", cond, my_tid);

      // IMPORTANT: If the threads are unblocked by cond_broadcast(), they will contend for the mutex
      // We have to release the mutex so that all blocked threads can wake up 
      // DEBUG_QTHREAD("# CondLockRel: %lu\n", my_tid);
      ppthread_mutex_unlock(&_cond_mutex);

      // Pass here one-by-one
      //waitToken();

      // Re-evaluate the activity after being woken up. If I should wake up, can progress
      ThreadEntry * entry = getActiveEntry(my_tid);
      if (entry == NULL) {
        DEBUG_QTHREAD("# CondFalseWakeup: %lu\n", my_tid);
        //passToken();
      } else {
        break;
      }
    }

    // If we arrive here , thread is determined to wake up
    // If woken by cond_signal(), only one thread will arrive here.
    // If woken by cond_broadcast(), more than one thread will arrive here.
    DEBUG_QTHREAD("# CondOK(%p): %lu\n", cond, my_tid);
    //passToken();


    // All the unblocked threads will acquire the lock deterministically
    LockAcquire(cond_mutex);

    return retval;
  }



  // Only one thread in the waiting list is woken up (if any threads are blocked on cond)
  int CondSignal(pthread_cond_t * cond) {
    int retval = -1;

    waitToken();

    bool isFound = false;

    // Look for the first cond waiting thread in the sleep entry
    // And move it back to the active entry
    for (unsigned int i = 0; i < _sleep_entries.size(); i++) {
      ThreadEntry entry = _sleep_entries[i];  // Deep copy
      if (entry.cond == (void *) cond) {
        isFound = true;
        // Activate thread
        DEBUG_QTHREAD("# CondAct(%lu): %lu\n", entry.tid, my_tid);
        // entry.status = STATUS_READY;   // set ready
        entry.cond = NULL;
        _active_entries.push_back(entry);  
        _sleep_entries.erase(_sleep_entries.begin() + i);
        break;   
      }
    }


    // Pass token to the thread that should be woken up
    if (isFound) {

      // Pass token to the one that should be woken-up
      _token_pos = _active_entries.size() - 1;
      __asm__ __volatile__ ("mfence");
      _token_tid = _active_entries.back().tid;
      DEBUG_QTHREAD("# throwToken(%lu): %lu\n", _token_tid, my_tid);

      // Signal after throwing token
      DEBUG_QTHREAD("# CondBSignal(%p): %lu\n", cond, my_tid);
      retval = ppthread_cond_broadcast(cond);
      //retval = ppthread_cond_signal(cond);

    } else {
      // No one is waiting on this cond, just pass the token and do not signal
      passToken();
    }


    // // Pass token to the one that should be woken-up
    // _token_pos = _active_entries.size() - 1;
    // _token_tid = _active_entries.back().tid;
    // DEBUG_QTHREAD("# throwToken(%lu): %lu\n", _token_tid, my_tid);

    // DEBUG_QTHREAD("# CondSignal(%p): %lu\n", cond, my_tid);

    // // We hope all the relavants can wake up and check its activity 
    // retval = ppthread_cond_broadcast(cond);

    // passToken();

    return retval;
  }


  int CondBroadcast(pthread_cond_t * cond) {
    int retval = -1;

    waitToken();

    int first_token_pos = -1;

    // Recover all the corresponding threads from sleep list to active list
    for (unsigned int i = 0; i < _sleep_entries.size(); ) {
      ThreadEntry entry = _sleep_entries[i]; // Deep cocy
      if (entry.cond == (void *) cond) {
        DEBUG_QTHREAD("# CondAct(%lu): %lu\n", entry.tid, my_tid);
        entry.status = STATUS_READY;   // set ready
        entry.cond = NULL;
        _active_entries.push_back(entry);
        _sleep_entries.erase(_sleep_entries.begin() + i);
        // Remember the thread ('s token pos) who will wakeup firstly 
        if (first_token_pos == -1) {
          first_token_pos = _active_entries.size() - 1;
        }
      } else {
        i++; 
      }
    }

    // Pass token to the thread that should be woken up *at first*
    if (first_token_pos != -1) {

      _token_pos = first_token_pos;

      __asm__ __volatile__ ("mfence");

      _token_tid = _active_entries[_token_pos].tid;
      DEBUG_QTHREAD("# throwToken(%lu): %lu\n", _token_tid, my_tid);

      DEBUG_QTHREAD("# CondBroadcast(%p): %lu\n", cond, my_tid);
      retval = ppthread_cond_broadcast(cond);
    } else {
      // No thread is waiting for that cond
      passToken();
    }

    //DEBUG_QTHREAD("# CondBroadcast(%p): %lu\n", cond, my_tid);
    //retval = ppthread_cond_broadcast(cond);

    // passToken();

    return retval;
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



  // Print the active list and sleep list elegantly
  void Bla(void) {

    // printf("\n* Token * ");
    // printf("%lu [%d]", _token_tid, _token_pos);

    printf("\n* Active * ");
    for (unsigned int i = 0; i < _active_entries.size(); i++) {
      if (_token_pos == (int) i) {
        printf("[%lu]   ", _active_entries[i].tid);  // having token
      } else {
        printf("%lu   ", _active_entries[i].tid);
      }
    }

    printf("\n* Sleep * ");
    for (unsigned int i = 0; i < _sleep_entries.size(); i++) {
      printf("%lu   ", _sleep_entries[i].tid);
    }

    printf("\n\n");
    return;
  }


  // Print the active list and sleep list in detail
  void Blabla(void) {
    printf("\n* Token tid ");
    printf("%lu", _token_tid);

    printf("\n* Token pos ");
    printf("%d", _token_pos);


    printf("\n* Active *\ntid\tstatus\tcond\tlock\tjoinee\tpid\n");
    for (unsigned int i = 0; i < _active_entries.size(); i++) {
      ThreadEntry e = _active_entries[i];
      printf("%lu\t%d\t%p\t%p\t%lu\t%lu\n", e.tid, e.status, e.cond, e.lock, e.joinee_tid, e.pid);
    }

    printf("\n* Sleep *\ntid\tstatus\tcond\tlock\tjoinee\tpid\n");
    for (unsigned int i = 0; i < _sleep_entries.size(); i++) {
      ThreadEntry e = _sleep_entries[i];
      printf("%lu\t%d\t%p\t%p\t%lu\t%lu\n", e.tid, e.status, e.cond, e.lock, e.joinee_tid, e.pid);
    }

    printf( "\n\n");
    return;
  }






private:



  ///////////////////////////////////////////////////////////////////// Token passing primitives
  // Busy waiting until the caller get the token
  inline void waitToken(void) const {

    // NOTE: It is reasonable to call waitToken() when there is no active thread at all
    // Because we assume the next behavior is to register/activate that thread
    if (_active_entries.empty()) 
      return;

    DEBUG_QTHREAD("# WaitToken: %lu\n", my_tid);

    while (my_tid != _token_tid) {
      __asm__ __volatile__ ("mfence");
    }
    //DEBUG_QTHREAD("# GetToken: %lu\n", my_tid);
    return;
  }


  // Force token holder yield the token to the next thread in the active list
  // NOTE: If the active list is empty, it will return immediately.
  inline void passToken(void) {

    if (_active_entries.empty()) {
      __asm__ __volatile__ ("mfence");
      return;
    }

    assert(_token_tid == my_tid);

    // IMPORTANT: Using the randomized passToken() will make spinlock() non-deterministic
    if (_random_next) {
      _token_pos = (_token_pos + 1 + KISS % _active_entries.size()) % _active_entries.size();
    } else {
      _token_pos = (_token_pos + 1) % _active_entries.size();
    }

    __asm__ __volatile__ ("mfence");

    _token_tid = _active_entries[_token_pos].tid;


    DEBUG_QTHREAD("# PassToken(->%lu): %lu\n", _token_tid, my_tid);
    
    return;
  }


  int activateThread(size_t tid) {
    // Locate the target thread in the entires
    for (unsigned int i = 0; i < _sleep_entries.size(); i++) {
      if (_sleep_entries[i].tid == tid) {
        DEBUG_QTHREAD("# Activate: %lu\n", tid);
        ThreadEntry entry = _sleep_entries[i]; // Deep copy
        entry.status = STATUS_READY;
        _active_entries.push_back(entry);
        _sleep_entries.erase(_sleep_entries.begin() + i);

        // Start from a empty list
        if (_token_pos == -1) {
          _token_pos = 0;
          _token_tid = tid;
          DEBUG_QTHREAD("# StartToken(%lu): %lu\n", tid, my_tid);
        }
        return 0;
      }
    }

    
    // if no entry is found
    DEBUG_QTHREAD("# Act404: %lu\n", tid);
    return 1;
  }




  // Move a specific thread from _active_entries to _sleep_entrires
  // Any sleeping threads must be assigned a status
  int deactivateThread(size_t tid, int status) {
    // Locate the target thread in the entires
    for (unsigned int i = 0; i < _active_entries.size(); i++) {
      if (_active_entries[i].tid == tid) {
        DEBUG_QTHREAD("# Deact(%lu): %lu\n", tid, my_tid);
        ThreadEntry entry = _active_entries[i]; // Deep copy
        entry.status = status;
        _sleep_entries.push_back(entry);
        _active_entries.erase(_active_entries.begin() + i);
        // NOTE: here we just adjust token pos back and do not change _token_tid
        if (_active_entries.empty()) {
          _token_pos = -1;
          DEBUG_QTHREAD("# CallbackToken: %lu\n", my_tid);
        } else { 
          _token_pos = (_token_pos - 1 + _active_entries.size()) % _active_entries.size();  // Roll back the pos
        }        
        return 0;
      }
    }
    // if  no entry is found
    DEBUG_QTHREAD("# Deact404(%lu): %lu\n", tid, my_tid);
    return 1;
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



  // inline void lock_(void) {
  //   ppthread_mutex_lock(&_mutex);
  //   return;
  // }

  // inline void unlock_(void) {
  //   ppthread_mutex_unlock(&_mutex);
  //   return;
  // }


};



/**
 * Fake entry point of thread. We use it to unregister thread inside the thread body
 */
void * fake_thread_entry(void * param) {

  ThreadParam * obj = static_cast<ThreadParam *>(param);
  
  // Dump parameters
  ThreadFunction my_func = obj->func;
  void * my_arg = (void *) obj->arg;
  size_t tid = obj->tid;  // We set my_tid before the thread body

  // Unlock after copying out paramemters
  ppthread_mutex_unlock(&g_spawn_lock);


  my_tid = tid;
  // Call the real thread function
  void * retval = my_func(my_arg);

  // Let each thread deregister it self
  Qthread::GetInstance().Terminate();


  return retval;
}


#endif
