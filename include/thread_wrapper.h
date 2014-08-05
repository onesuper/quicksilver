#pragma once


extern pthread_mutex_t g_lock;

// Pack all the parameters in a struct
class ThreadParam {
public:
  volatile ThreadFunction *func;  
  volatile size_t index;
  volatile void *arg;
	
	ThreadParam(): func(NULL), index(0), arg(NULL) {}
};


// Used to pass thread's parameters when spawning thread
ThreadParam thread_param;



// Fake entry point of thread. We use it to unregister thread inside the thread body
void *ThreadFuncWrapper(void *param) {
  
  ThreadParam *obj = (ThreadParam *) param;
   
  my_tid = obj->index;
  ThreadFunction *my_func = obj->func;
  void *my_arg = (void *) obj->arg;

  // Unlock after copying out paramemters
  ppthread_mutex_unlock(&g_lock);

  // Call the real thread function
  void* retval = my_func(my_arg);

  // Let each thread deregister it self
  Qthread::GetInstance().DeregisterThread(my_tid);

  return retval;
}

