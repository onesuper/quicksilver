#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;


void do_something(void) {
  int i, a;
  for (i = 0; i < 1000; i++) a++;
}


void *worker(void *data) {
  pthread_mutex_lock(&g_lock);

  do_something();

  pthread_mutex_unlock(&g_lock);

  pthread_exit(0);

  //return NULL;
}


int main(int argc, char **argv) {
  const int nThreads = 2;
  pthread_t workers[nThreads];
  int i;

  for (i = 0; i < nThreads; i++)
    pthread_create(&workers[i], NULL, worker, NULL);

  for (i = 0; i < nThreads; i++)
    pthread_join(workers[i], NULL);

  return 0;
}
