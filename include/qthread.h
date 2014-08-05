#pragma once

#include <vector>
#include <new>
#include <sched.h>

#include "ppthread.h" 
#include "debug.h"
#include "stat.h"


// Thread local storage: use to let thread know who he is
__thread size_t my_tid;


class Qthread {

private:
  // Used to manage threads
  struct ThreadEntry {
    size_t tid;
    ThreadEntry() {}
    ThreadEntry(size_t _tid): tid(_tid) {}
  };

  // Maintain all the active thread entries in a vector 
  std::vector<ThreadEntry> _active_entries;

  // Pointing to the thread entry who holds the token
  // Could be negative (-1 = no one has token)
  volatile int _token_pos;

  // All threads use this lock to prevent from data race.
  pthread_mutex_t _mutex;


  // Statistical info
  Stat _stat_total;
  Stat _stat_serial;
  

public:

  Qthread():
  _token_pos(-1),
	_active_entries(),
  _stat_total(),
  _stat_serial()
  {}

  // Singleton pattern
  static Qthread& GetInstance(void) {
    static Qthread *obj = NULL;
    if (obj == NULL) { 
      obj = new Qthread();
    }
    return *obj;
  }

  void Init() {

    _stat_total.Start();
    ppthread_mutex_init(&_mutex, NULL);
    printActiveEntries();
    whoHasToken();
    return;
  }

  void Destroy() {
    _stat_total.Pause();

    printf("Total Time: %ld\n", _stat_total.Total());
    printf("Serial Time: %ld @ %ld\n", _stat_serial.Total(), _stat_serial.Times()); 
 
    return;
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
    }

    printActiveEntries();
    whoHasToken();
    unlock();

    return;
  }

  void DeregisterThread(size_t index) {

    DEBUG("Deregistering thread %lu ...", index);
    spinForToken();
    lock();
    
    // Locate the target thread in the entires
    std::vector<ThreadEntry>::size_type pos;
    for (pos=0; pos<_active_entries.size(); pos++) {
      if (_active_entries[pos].tid == index) {
        break;   
      }
    }
   
    // Check whether the target thread holds token
    if (pos != _token_pos) { 
      unlock();
      DEBUG("Thread %lu tried to pass token but the token belongs to %lu", index, tokenIndex());
      assert(0);
      return;
    } 

    // Delete the entry from the active list. The next entry gets the token for free 
    _active_entries.erase(_active_entries.begin() + pos);

    // Decide the token ownership
    // Get back the token 
    if (_active_entries.empty()) { 
      _token_pos = -1;
      DEBUG("Since there is no thread left, token is called back");
      unlock();
      return;
    } 
      				
    // loop back to the first position
    if (pos == _active_entries.size()) { 
      _token_pos = 0;
    }

    DEBUG("%lu is deregisterred, token is passed to %lu", index, tokenIndex());

    printActiveEntries();
    whoHasToken();

    unlock();
    return;
  }



  int MutexInit(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    return ppthread_mutex_init(mutex, attr);
  }

  int MutexLock(pthread_mutex_t *mutex) {

    _stat_serial.Start();
    int retval = -1;

    // Wait for the token
    while (true) {				  
      // Acqurie the lock anyway
      retval = ppthread_mutex_lock(mutex);
      DEBUG("Thread %lu grabs the token", my_tid);
		  
      // Check whether I have the token
      if (my_tid != tokenIndex()) {
        // if I have no token, yield the lock
        DEBUG("Thread %lu yields the lock since he has no token (%lu)", my_tid, tokenIndex());
        ppthread_mutex_unlock(mutex); 
      } else {
        break;
      }
    }    

    _stat_serial.Pause();        
    passToken();
    return retval;
  }

  int MutexUnlock(pthread_mutex_t *mutex) {
    return ppthread_mutex_unlock(mutex);
  }

  int MutexTrylock(pthread_mutex_t *mutex) {

    //int retval = ppthread_mutex_trylock(mutex);

    return 0;
  }


  int MutexDestroy(pthread_mutex_t *mutex) {
    return ppthread_mutex_destroy(mutex);
  }

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

  void spinForToken(void) {
    DEBUG("<%lu> spin for token (in <%lu>'s hand)", my_tid, tokenIndex());
    while (my_tid != tokenIndex()) {
      //sched_yield();
    }
    DEBUG("Thread %lu gets the token", my_tid);

    _stat_serial.Start();
    return;
  }


  // Force thread tid pass his token to the next thread in the active list
  void passToken(void) {

    assert(_token_pos >= 0);


    lock();

    // Do I have token?
    if (my_tid != tokenIndex()) {
      unlock();
      DEBUG("Error! <%lu> attempts to pass token but the token belongs to %lu", my_tid, tokenIndex());
      assert(0);
      return;
    }

    _token_pos = (_token_pos + 1) % _active_entries.size();
    DEBUG("<%lu> Token is now passed to %lu", my_tid, tokenIndex());

    unlock();
    return;
  }


  size_t tokenIndex() {
    assert(_token_pos >= 0);
    assert(_token_pos < _active_entries.size());
    return _active_entries[_token_pos].tid;
  }


  void whoHasToken(void) {
    DEBUG("Who has the token?");
    if (_token_pos >= 0) {
      DEBUG("%lu @ [%d]", tokenIndex(), _token_pos);
    } else {
      DEBUG("NIL");
    }
    return;
  }

  
  void printActiveEntries(void) {
    DEBUG("Printing the active entries...")
    std::vector<ThreadEntry>::size_type i;
    for (i=0; i<_active_entries.size(); i++) {
      DEBUG("%lu", _active_entries[i].tid);
    }
    return;
  }

public:
  inline void lock(void) {
    ppthread_mutex_lock(&_mutex);
    return;
  }

  inline void unlock(void) {
    ppthread_mutex_unlock(&_mutex);
    return;
  }




};
