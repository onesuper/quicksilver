#pragma once


#include <new>
#include <sched.h>
#include "ppthread.h"  // our pthread header
#include "list.h"
#include "thread_entry.h"
#include "thread_private.h"
#include "debug.h"
#include "stat.h"

#define MAX_THREADS 2048


class Qthread {

private:
  // Register all the active threads into this list.
  List _activelist;

  // All threads use this lock to prevent from data race.
  pthread_mutex_t        _mutex;
  pthread_mutexattr_t    _mutexattr;

  // Pointing to the thread entry who holds the token
  ThreadEntry *_token_entry;

  // ThreadEntry lookup table, hash the thread in this table by index
  ThreadEntry _thread_entries[MAX_THREADS];

  size_t _max_thread_entries;


  // Statistical info
  Stat _stat_total;
  Stat _stat_serial;

public:

  Qthread():
    _token_entry(NULL),
    _max_thread_entries(MAX_THREADS),
    _stat_total(),
    _stat_serial()
    {}

  // Singleton pattern
  static Qthread& getInstance(void) {
    static Qthread *obj = NULL;
    if (obj == NULL) { 
      obj = new Qthread();
    }
    return *obj;
  }

  void init() {
  

    _stat_total.start();  
    whoHasToken();
    _activelist.print();
    // Obtaining the pthread resources first
    Pthread::getInstance().init();
    Pthread::getInstance().mutexattr_init(&_mutexattr);
    Pthread::getInstance().mutex_init(&_mutex, &_mutexattr);
  }
  
  
  void del() {
    _stat_total.pause();

  	DEBUG("Total Time: %ld", _stat_total.getTotal());
  	DEBUG("Serial Time: %ld @ %ld", _stat_serial.getTotal(), _stat_serial.getTimes());  

    whoHasToken();
    _activelist.print();
    // Cleaning the pthread resources first
    Pthread::getInstance().mutex_destroy(&_mutex);
    Pthread::getInstance().del();
  }
  
  
  
  void waitForToken(void) {
    DEBUG("<%d> wait for token (in <%d>'s hand)", my_tid(), _token_entry->getIndex());
    while (my_tid() != _token_entry->getIndex()) {
      sched_yield();
    }
    DEBUG("Thread %d gets the token", my_tid());
    
    _stat_serial.start();
    
    return;
  }



  // Force thread tid pass his token to the next thread in the active list
  void passToken(void) {

	  assert(_token_entry != NULL);
	  
	  _stat_serial.pause();
	  
    lock();
    // Do I have token?
    if (my_tid() != _token_entry->getIndex()) {
      unlock();
      ERROR("Error! <%d> attempts to pass token but the token belongs to %d", my_tid(), _token_entry->getIndex());
      assert(0);
    }

    ThreadEntry *next = (ThreadEntry *) _token_entry->next;
    _token_entry = next;
    DEBUG("<%d> Token is now passed to %d", my_tid(), _token_entry->getIndex());

    unlock();

    return;
  }


	void whoHasToken(void) {
		DEBUG("Who has the token?");
		
		if (_token_entry != NULL) {
    	_token_entry->print();
    } else {
      DEBUG("No one!");
    }
	}


  void registerThread(int tid) {

    lock();
    
    DEBUG("Registering thread %d ...", tid);

    // Create a new threadEntry in the memory already allocated  
    // allocThreadEntry() just return a position in an array
    void *ptr = allocThreadEntry(tid);
    ThreadEntry *entry = new (ptr) ThreadEntry(tid);

    // Link the newly created threadEntry into the active thread list
    _activelist.insertTail(entry);

    // Print out the active list
    _activelist.print();

    // Initially, token belongs to the first created thread
    if (_token_entry == NULL) {
      _token_entry = entry;
    }

    whoHasToken();
    unlock();
  }

  void deregisterThread(int index) {

    waitForToken();
    DEBUG("Deregistering thread %d ...", index);
        
	  ThreadEntry* entry = &_thread_entries[index];
    ThreadEntry* next = (ThreadEntry *) entry->next;
    lock();    
    
    // Remove the entry from active list
		_activelist.remove(entry);
		
    _activelist.print();
    
    // Free the thread entry
    freeThreadEntry(entry);
	  
    // After deleting thread entry, pass the token (if hold).
    if (index != _token_entry->getIndex()) { // Check whether holds token
      unlock();
      ERROR("Thread %d tried to pass token but the token belongs to %d", index, _token_entry->getIndex());
    } else {
      // Try to pass the token
      if (entry == next) { // Get back the token if there's only one thread before deleting
        _token_entry = NULL;
        unlock();
        DEBUG("Since there is no thread left, token is called back");
      } else { // Else pass the token
        _token_entry = next;
        unlock();
        DEBUG("Since %d is deregisterred, token is passed to %d", index, _token_entry->getIndex());
      }
    }


  }

  int exit(void *val_ptr) {
  
    // Deregister the thread if user call exit() manually in his code
    
    deregisterThread(my_tid());
 
    DEBUG("Call real pthread_exit");
    return Pthread::getInstance()._exit(val_ptr);
  }

  int cancel(pthread_t tid) {
    DEBUG("Call real pthread_cancel");
    return Pthread::getInstance().cancel(tid);
  }

  int join(pthread_t tid, void **val) {
    DEBUG("Call real pthread_join");
    return Pthread::getInstance().join(tid, val);
  }

  int mutexattr_init(pthread_mutexattr_t *attr) {
    DEBUG("Call real pthread_mutexattr_init");
    return Pthread::getInstance().mutexattr_init(attr);
  }

  int mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    DEBUG("Call real pthread_mutex_init");
    return Pthread::getInstance().mutex_init(mutex, attr);
  }

  int mutex_lock(pthread_mutex_t *mutex) {
    waitForToken();
    DEBUG("<%d> call real pthread_mutex_lock", my_tid());
    int retval = Pthread::getInstance().mutex_lock(mutex);
    passToken();
    return retval;
  }

  int mutex_unlock(pthread_mutex_t *mutex) {
    DEBUG("<%d> call real pthread_mutex_unlock", my_tid());
    return Pthread::getInstance().mutex_unlock(mutex);
  }

  int mutex_trylock(pthread_mutex_t *mutex) {
    DEBUG("Call real pthread_mutex_trylock");
    return Pthread::getInstance().mutex_trylock(mutex);
  }

  int mutex_destroy(pthread_mutex_t *mutex) {
    DEBUG("Call real pthread_mutex_destroy");
    return Pthread::getInstance().mutex_destroy(mutex);
  }

  int condattr_init(pthread_condattr_t *attr) {
    DEBUG("Call real pthread_condattr_init");
    return Pthread::getInstance().condattr_init(attr);
  }

  int cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    DEBUG("Call real pthread_cond_init");
    return Pthread::getInstance().cond_init(cond, attr);
  }

  int cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    DEBUG("Call real pthread_cond_wait");
    return Pthread::getInstance().cond_wait(cond, mutex);
  }

  int cond_signal(pthread_cond_t *cond) {
    DEBUG("Call real pthread_cond_signal");
    return Pthread::getInstance().cond_signal(cond);
  }

  int cond_broadcast(pthread_cond_t *cond) {
    DEBUG("Call real pthread_cond_broadcast");
    return Pthread::getInstance().cond_broadcast(cond);
  }

  int cond_destroy(pthread_cond_t *cond) {
    DEBUG("Call real pthread_cond_destroy");
    return Pthread::getInstance().cond_destroy(cond);
  }

  int barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count) {
    DEBUG("Call real pthread_barrier_init");
    return Pthread::getInstance().barrier_init(barrier, attr, count);
  }

  int barrier_wait(pthread_barrier_t *barrier) {
    DEBUG("Call real pthread_barrier_wait");
    return Pthread::getInstance().barrier_wait(barrier);
  }

  int barrier_destroy(pthread_barrier_t *barrier) {
    DEBUG("Call real pthread_barrier_destroy");
    return Pthread::getInstance().barrier_destroy(barrier);
  }




  inline void lock(void) {
    Pthread::getInstance().mutex_lock(&_mutex);
  }

  inline void unlock(void) {
    Pthread::getInstance().mutex_unlock(&_mutex);
  }

  inline void *allocThreadEntry(int thread_index) {
    assert(thread_index < _max_thread_entries);
    return (&_thread_entries[thread_index]);
  }

  inline void freeThreadEntry(void *entry) {
    // do nothing here
    return;
  }





};
