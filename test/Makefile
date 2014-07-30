
LIB_DIR=../lib
CFLAGS = -Wall -g -O3 
PTHREAD_LIBS = -lpthread  
QTHREAD_DYNAMIC = $(abspath $(LIB_DIR)/libqthread.so) -rdynamic -lstdc++ -ldl -lpthread
QTHREAD_STATIC = $(abspath $(LIB_DIR)/libqthread.a) -lstdc++ -ldl -lpthread 

CC = gcc

all: dlink pthread slink

pthread:
	mkdir -p ./pthread
	$(CC) $(CFLAGS) test_mutex.c        -o ./pthread/test_mutex        $(PTHREAD_LIBS)
	$(CC) $(CFLAGS) test_thread_index.c -o ./pthread/test_thread_index $(PTHREAD_LIBS)
	$(CC) $(CFLAGS) racey.c             -o ./pthread/racey             $(PTHREAD_LIBS)


dlink:
	mkdir -p ./dlink
	$(CC) $(CFLAGS) test_mutex.c        -o ./dlink/test_mutex        $(QTHREAD_DYNAMIC)
	$(CC) $(CFLAGS) test_thread_index.c -o ./dlink/test_thread_index $(QTHREAD_DYNAMIC)
	$(CC) $(CFLAGS) racey.c             -o ./dlink/racey             $(QTHREAD_DYNAMIC)


slink:
	mkdir -p ./slink
	$(CC) $(CFLAGS)  test_mutex.c        -o ./slink/test_mutex        $(QTHREAD_STATIC)
	$(CC) $(CFLAGS)  test_thread_index.c -o ./slink/test_thread_index $(QTHREAD_STATIC)
	$(CC) $(CFLAGS)  racey.c             -o ./slink/racey             $(QTHREAD_STATIC)

  
clean:
	rm -rf ./pthread ./slink ./dlink
