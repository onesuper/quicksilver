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
  printf("%lu is entering LockA's section\n", pthread_self());


  pthread_mutex_unlock(&lockA);

  do_something();

  return NULL;
}


void *thread2(void *data) {

  do_something();

  pthread_mutex_lock(&lockA);

  do_something();

  printf("%lu is in LockA's section\n", pthread_self());

  pthread_mutex_lock(&lockB);

  do_something();

  printf("%lu is entering LockB's section\n", pthread_self());

  pthread_mutex_unlock(&lockB);

  do_something();

  printf("%lu is in another LockA's section\n", pthread_self());

  pthread_mutex_unlock(&lockA);

  do_something();
  
  return NULL;
}


int main(int argc, char **argv) {

  pthread_mutex_init(&lockA, NULL);
  pthread_mutex_init(&lockB, NULL);

  pthread_t worker0;

  pthread_t worker1;

  pthread_create(&worker0, NULL, thread2, NULL);
  pthread_create(&worker1, NULL, thread1, NULL);


  pthread_join(worker0, NULL);
  pthread_join(worker1, NULL);

  return 0;
}
