#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>


/* All threads use this lock to protect critical section */
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

    printf("%lu is entering critial section %d\n", pthread_self(), i);

    do_something();
    sleep(1);

    printf("%lu is leaving critial section %d\n", pthread_self(), i);
    pthread_mutex_unlock(&g_lock);

    do_something();
    //sleep(1);
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

  qthread_leave_game();

  for (i = 0; i < nThreads; i++)
    pthread_join(workers[i], NULL);

//  pthread_join_game();

  return 0;
}
