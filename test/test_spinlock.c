#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

pthread_spinlock_t g_lock;


void do_something(void) {
  int i, a;
  for (i = 0; i < 100000; i++) a++;
}


void *worker(void *data) {
  pthread_spin_lock(&g_lock);

  do_something();

  pthread_spin_unlock(&g_lock);

  do_something();
  //pthread_exit(0);

  return NULL;
}


int main(int argc, char **argv) {

  pthread_spin_init(&g_lock, PTHREAD_PROCESS_PRIVATE);

  const int nThreads = 4;
  pthread_t workers[nThreads];
  int i;

  for (i = 0; i < nThreads; i++)
    pthread_create(&workers[i], NULL, worker, NULL);

  for (i = 0; i < nThreads; i++)
    pthread_join(workers[i], NULL);

  return 0;
}
