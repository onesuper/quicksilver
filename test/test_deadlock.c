#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

pthread_mutex_t lockA;
pthread_mutex_t lockB;


void do_something(void) {
  int i, a;
  for (i = 0; i < 1000000; i++) a++;
}

void *thread1(void *data) {

  do_something();

  pthread_mutex_lock(&lockA);

  do_something();

  pthread_mutex_unlock(&lockA);

  do_something();

  return NULL;
}


void *thread2(void *data) {

  do_something();

  pthread_mutex_lock(&lockA);

  do_something();

  pthread_mutex_lock(&lockB);

  do_something();

  pthread_mutex_lock(&lockB);

  do_something();

  pthread_mutex_unlock(&lockA);

  do_something();
  
  return NULL;
}


int main(int argc, char **argv) {

  pthread_mutex_init(&lockA, NULL);
  pthread_mutex_init(&lockB, NULL);

  pthread_t workers[2];

  pthread_create(&workers[0], NULL, thread1, NULL);
  pthread_create(&workers[1], NULL, thread2, NULL);


  ppthread_join(workers[0], NULL);
  ppthread_join(workers[1], NULL);

  return 0;
}
