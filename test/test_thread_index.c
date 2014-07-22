#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>


void *worker(void *data) {
  
  printf("self=%d, pid=%d, tid=%d\n", pthread_self(), getpid(), gettid());
  return NULL;
}


int main(int argc, char **argv) {
  const int nThreads = 4;
  pthread_t workers[nThreads];
  int i;

  for (i = 0; i < nThreads; i++)
    pthread_create(&workers[i], NULL, worker, NULL);

  for (i = 0; i < nThreads; i++)
    pthread_join(workers[i], NULL);

  return 0;
}
