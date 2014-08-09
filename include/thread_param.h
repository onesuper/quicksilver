#ifndef _THREAD_PARAM_H_
#define _THREAD_PARAM_H_


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





#endif