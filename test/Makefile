

CFLAGS = -Wall -g -O3 
PTHREAD_LIBS = -lpthread  
QTHREAD_LIBS = -rdynamic $(abspath ../lib/libqthread.so)
CC = gcc


all: qthread pthread

pthread:
	mkdir -p ./pthread
	$(CC) $(CFLAGS) test_mutex.c -o        ./pthread/test_mutex        $(PTHREAD_LIBS)
	$(CC) $(CFLAGS) test_thread_index.c -o ./pthread/test_thread_index $(PTHREAD_LIBS)
	$(CC) $(CFLAGS) racey.c -o             ./pthread/racey             $(PTHREAD_LIBS)


qthread:
	mkdir -p ./qthread
	$(CC) $(CFLAGS) test_mutex.c -o        ./qthread/test_mutex        $(QTHREAD_LIBS)
	$(CC) $(CFLAGS) test_thread_index.c -o ./qthread/test_thread_index $(QTHREAD_LIBS)
	$(CC) $(CFLAGS) racey.c -o             ./qthread/racey             $(QTHREAD_LIBS)
	

  
clean:
	rm -rf ./pthread ./qthread
