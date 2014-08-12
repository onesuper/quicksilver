#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

pthread_mutex_t g_lock;


void do_something(void) {
  int i, a;
  for (i = 0; i < 100000; i++) a++;
}


void *worker(void *data) {

  int i;
  for (i = 0; i < 5; i++) {
    do_something();

    pthread_mutex_lock(&g_lock);

    do_something();

    printf("%lu is entering critial section %d\n", pthread_self(), i);

    pthread_mutex_unlock(&g_lock);

    do_something();
  }
  
  return NULL;
}


int main(int argc, char **argv) {

  pthread_mutex_init(&g_lock, NULL);

  const int nThreads = 4;
  pthread_t workers[nThreads];
  int i;

  for (i = 0; i < nThreads; i++)
    pthread_create(&workers[i], NULL, worker, NULL);

  for (i = 0; i < nThreads; i++)
    pthread_join(workers[i], NULL);

  return 0;
}
