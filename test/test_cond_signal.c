#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#define Q

int g_signaled = 0;
pthread_mutex_t g_lock;
pthread_cond_t g_cond;



void *wait(void *data) {

  pthread_mutex_lock(&g_lock);

  // double check
  while (g_signaled == 0) {
    printf("%lu wait cond\n", pthread_self());
    pthread_cond_wait(&g_cond, &g_lock); // we wake up here, and break out the loop
  }

  pthread_mutex_unlock(&g_lock);


  printf("%lu bang!\n", pthread_self());

  return NULL;
}


void *signal(void *data) {


  pthread_mutex_lock(&g_lock);
  g_signaled = 1;
  pthread_mutex_unlock(&g_lock);
  

  printf("send out signal!\n");


  pthread_cond_signal(&g_cond);

  sleep(1);


  pthread_cond_signal(&g_cond);

  sleep(1);
  pthread_cond_signal(&g_cond);

  sleep(1);


  pthread_cond_signal(&g_cond);
  sleep(1);


  pthread_cond_signal(&g_cond);


  return NULL;
}


int main(int argc, char **argv) {


  pthread_mutex_init(&g_lock, NULL);
  pthread_cond_init(&g_cond, NULL);


  const int nThreads = 5;
  pthread_t workers[nThreads];
  int i;

  pthread_create(&workers[0], NULL, wait, NULL);
  pthread_create(&workers[1], NULL, wait, NULL);
  pthread_create(&workers[2], NULL, wait, NULL);
  pthread_create(&workers[3], NULL, wait, NULL);
  pthread_create(&workers[4], NULL, signal, NULL);
  //pthread_create(&workers[3], NULL, signal, NULL);


#ifdef Q
  qthread_hibernate_thread(0);
#endif

  for (i = 0; i < nThreads; i++)
    pthread_join(workers[i], NULL);

  return 0;
}
