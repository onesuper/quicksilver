#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>


/* Main thread is assigned an ID */

/* All threads use this lock to protect critical section */
pthread_mutex_t g_lock;


void do_something(void) {
  int i, a;
  for (i = 0; i < 100000; i++) a++;
}


void *child(void *data) {

  int i;
  for (i = 0; i < 5; i++) {
    do_something();

    pthread_mutex_lock(&g_lock);

    printf("child %lu is entering critial section %d\n", pthread_self(), i);
    do_something();
    sleep(1);
    printf("child %lu is leaving critial section %d\n", pthread_self() , i);

    pthread_mutex_unlock(&g_lock);

    do_something();
  }
  
  return NULL;
}


int main(int argc, char **argv) {

  pthread_mutex_init(&g_lock, NULL);

  pthread_t worker;

  pthread_create(&worker, NULL, child, NULL);

  int i;
  for (i=0; i<5; i++) {

    do_something();

    pthread_mutex_lock(&g_lock);


    printf("main %lu is entering critial section %d\n", pthread_self(), i);
    do_something();
    sleep(1);
    printf("main %lu is leaving critial section %d\n", pthread_self() , i);

    pthread_mutex_unlock(&g_lock);

    do_something();
  }

  // this is for main's leave main
  //qthread_leave_game();

  pthread_join(worker, NULL);



  return 0;
}
