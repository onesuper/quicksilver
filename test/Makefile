


CFLAGS = -Wall -g -O3 
PTHREAD_LIBS = -lpthread  
QTHREAD_LIBS = -rdynamic ../lib/libqthread.so
CC = gcc


all: qthread pthread racey

pthread:
	$(CC) $(CFLAGS) test_mutex.c -o test_mutex_p $(PTHREAD_LIBS)


qthread:
	$(CC) $(CFLAGS) test_mutex.c -o test_mutex_q $(QTHREAD_LIBS)


racey:
	$(CC) -m32 racey.c -o racey $(PTHREAD_LIBS)

clean:
	rm -rf test_mutex_p test_mutex_q 
