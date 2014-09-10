#ifndef _ENTRY_H_
#define _ENTRY_H_


class ThreadEntry {

public:

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


  ///////////////////////////////////////////////////////////////////// Lock Ownership  
  class LockOwnership {
  public:
    volatile unsigned int budget;
    volatile void * lock; // The address of the lock we are tracking

    inline LockOwnership(): budget(0), lock(NULL) {}

    inline LockOwnsership(const LockOwnsership & lo)  {
      this->budget = lo.budget;
      this->lock = lo.lock;
    }

  };

  volatile size_t tid;     // The thread we assigned
  volatile pthread_t pid;  // Assigned by pthread_self
  volatile int status;
  volatile void * cond;    // each thread may wait for one cond each time   
  volatile void * lock;
  volatile LockOwnership lock_ownership;  // each thread can own a lock

  // volatile void * barrier;
  // volatile bool broadcast; // If broadcast = true, the thread will be woken up by broadcast

  inline ThreadEntry() {}

  inline ThreadEntry(size_t tid, pthread_t pid) {
      this->tid = tid;
      this->pid = pid;
      this->status = STATUS_READY;
      this->cond = NULL;
      this->lock = NULL;
      this->lock_ownership = LockOwnership();
      // this->barrier = NULL;
  } 

  inline ThreadEntry(const ThreadEntry & entry) {
    this->tid = entry.tid;
    this->pid = entry.pid;
    this->status = entry.status;
    this->cond = entry.cond;
    this->lock = entry.lock;
    this->lock_ownership = entry.lock_ownership;
    // this->barrier = entry.barrier;
  }
  
};


#endif