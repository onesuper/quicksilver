#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

pthread_mutex_t g_lock;


void do_something(void) {
  int i, a;
  for (i = 0; i < 100000; i++) a++;
}


void *child(void *data) {

  do_something();

  printf("in child%lu\n", pthread_self());

  return NULL;
}

void *parent(void *data) {

  do_something();

  printf("in parent%lu\n", pthread_self());


  const int nThreads = 2;
  pthread_t workers[nThreads];
  int i;

  for (i = 0; i < nThreads; i++)
    pthread_create(&workers[i], NULL, child, NULL);

  for (i = 0; i < nThreads; i++)
    pthread_join(workers[i], NULL);

  do_something();

  return NULL;
}


int main(int argc, char **argv) {

  pthread_mutex_init(&g_lock, NULL);

  const int nThreads = 2;
  pthread_t workers[nThreads];
  int i;

  for (i = 0; i < nThreads; i++)
    pthread_create(&workers[i], NULL, parent, NULL);

  for (i = 0; i < nThreads; i++)
    pthread_join(workers[i], NULL);

  return 0;
}
