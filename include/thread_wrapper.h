#pragma once


// Pthread thread body
typedef void *Func(void *);

// Pack all the parameters in a struct
class ThreadParam {
public:
  volatile Func *func;  
  volatile size_t index;
  volatile void *arg;
	
	ThreadParam(): func(NULL), index(0), arg(NULL) {}
};



ThreadParam thread_param;

extern pthread_mutex_t spawn_lock;


// Fake entry point of thread. We use it to unregister thread inside the thread body
void *ThreadFuncWrapper(void *param) {
  
  // Dump parameters
  ThreadParam *obj = (ThreadParam *) param;
  my_tid = obj->index;
  Func *my_func = obj->func;
  void *my_arg = (void *) obj->arg;

  // Unlock after copying out paramemters
  ppthread_mutex_unlock(&spawn_lock);

  // Call the real thread function
  void* retval = my_func(my_arg);

  // Let each thread deregister it self
  Qthread::GetInstance().DeregisterThread(my_tid);

  return retval;
}

