#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

pthread_rwlock_t rwlock;


void do_something(void) {
  int i, a;
  for (i = 0; i < 100000; i++) a++;
}

void *reader1(void *);
void *reader2(void *);
void *writer1(void *);
void *writer2(void *);


int main(int argc, char **argv) {

  pthread_rwlock_init(&rwlock, NULL);

  pthread_t t0, t1, t2, t3;

  pthread_create(&t0, NULL, reader1, NULL);
  pthread_create(&t1, NULL, reader2, NULL);
  pthread_create(&t2, NULL, writer1, NULL);
  pthread_create(&t3, NULL, writer2, NULL);

  pthread_join(t0, NULL);
  pthread_join(t1, NULL);
  pthread_join(t2, NULL);
  pthread_join(t3, NULL);

  pthread_rwlock_destroy(&rwlock);
  return 0;
}

void *reader1(void *arg) {
  int i;
  for (i=0; i<3; i++) {
   do_something();
   pthread_rwlock_rdlock(&rwlock);
   printf("%lu is entering critical section %d as a reader\n", pthread_self(), i);
   do_something();
   sleep(1); 
   do_something();
   printf("%lu is leaving critical section %d as a reader\n", pthread_self(), i);
   pthread_rwlock_unlock(&rwlock); 
   do_something(); 
   sleep(1); 
 }
  return NULL;
}


void *reader2(void *arg) {
  int i;
  for (i=0; i<3; i++) {
    do_something();
    pthread_rwlock_rdlock(&rwlock);
    printf("%lu is entering critical section %d as a reader\n", pthread_self(), i);
    do_something();
    sleep(1); 
    do_something();
    printf("%lu is leaving critical section %d as a reader\n", pthread_self(), i);
    pthread_rwlock_unlock(&rwlock);
    do_something();
    sleep(1); 
  }
  return NULL;
}

void *writer1(void *arg) {
  int i;
  for (i=0; i<2; i++) {
    do_something();
    pthread_rwlock_wrlock(&rwlock);
    printf("%lu is entering critical section %d as a writer\n", pthread_self(), i);
    do_something();
    sleep(1); 
    do_something();
    printf("%lu is leaving critical section %d as a writer\n", pthread_self(), i);
    pthread_rwlock_unlock(&rwlock);
    do_something();
    sleep(1); 
  }
  return NULL;
}

void *writer2(void *arg) {

  int i;
  for (i=0; i<2; i++) {
    do_something();
    pthread_rwlock_wrlock(&rwlock);
    printf("%lu is entering critical section %d as a writer\n", pthread_self(), i);
    do_something();
    sleep(1); 
    do_something();
    printf("%lu is leaving critical section %d as a writer\n", pthread_self(), i);
    pthread_rwlock_unlock(&rwlock);
    do_something();
    sleep(1); 

  }
  return NULL;

}






