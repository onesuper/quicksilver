#ifndef _OWNERSHIP_H_
#define _OWNERSHIP_H_


// How soon can a lock be owned?
//#define LOCK_BUDGET 3

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


#endif