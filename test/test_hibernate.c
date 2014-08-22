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
  do_something();
  
  return NULL;
}


int main(int argc, char **argv) {


  pthread_mutex_init(&g_lock, NULL);

  const int nThreads = 2;
  pthread_t workers[nThreads];
  int i;

  for (i = 0; i < nThreads; i++)
    pthread_create(&workers[i], NULL, worker, NULL);

  qthread_hibernate_thread(0);

  qthread_hibernate_thread(1);
  qthread_wakeup_thread(1);


  for (i = 0; i < nThreads; i++)
    pthread_join(workers[i], NULL);




//  pthread_join_game();

  return 0;
}
