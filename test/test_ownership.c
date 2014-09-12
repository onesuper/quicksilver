#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

pthread_spinlock_t lockA;
pthread_spinlock_t lockB;
pthread_spinlock_t lockC;
pthread_spinlock_t lockD;

#define N 6



void do_something(void) {
  int i, a;
  for (i = 0; i < 100000; i++) a++;
}


void *workerA(void *data) {
  int i;
  for (i = 0; i < 6; i++) {
    do_something();
    pthread_spin_lock(&lockA);
    do_something();
    printf("%lu is entering critial section %d\n", pthread_self(), i);
    pthread_spin_unlock(&lockA);
    do_something();
  }
  return NULL;
}

void *workerB(void *data) {
  int i;
  for (i = 0; i < N; i++) {
    do_something();
    pthread_spin_lock(&lockB);
    do_something();
    printf("%lu is entering critial section %d\n", pthread_self(), i);
    pthread_spin_unlock(&lockB);
    do_something();
  }
  return NULL;
}

void *workerC(void *data) {
  int i;
  for (i = 0; i < N; i++) {
    do_something();
    pthread_spin_lock(&lockB);
    do_something();
    printf("%lu is entering critial section %d\n", pthread_self(), i);
    pthread_spin_unlock(&lockB);
    do_something();
  }
  return NULL;
}

void *workerD(void *data) {
  int i;
  for (i = 0; i < N; i++) {
    do_something();
    pthread_spin_lock(&lockB);
    do_something();
    printf("%lu is entering critial section %d\n", pthread_self(), i);
    pthread_spin_unlock(&lockB);
    do_something();
  }
  return NULL;
}


int main(int argc, char **argv) {

  pthread_spin_init(&lockA, NULL);
  pthread_spin_init(&lockB, NULL);
  pthread_spin_init(&lockC, NULL);
  pthread_spin_init(&lockD, NULL);

  pthread_t workers[4];


  pthread_create(&workers[0], NULL, workerA, NULL);
  pthread_create(&workers[1], NULL, workerB, NULL);
  pthread_create(&workers[2], NULL, workerC, NULL);
  pthread_create(&workers[3], NULL, workerD, NULL);


  pthread_join(workers[0], NULL);
  pthread_join(workers[1], NULL);
  pthread_join(workers[2], NULL);
  pthread_join(workers[3], NULL);




  return 0;
}
