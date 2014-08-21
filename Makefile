

CC = g++
CFLAGS = -m64 -DDEBUG  -g
DYNAMIC_LIB = -shared -fPIC 

all: libqthread.so libqthread.a

INC_DIR = include
SRC_DIR = src


libqthread.so: 
	mkdir -p lib
	$(CC) $(CFLAGS) -I$(INC_DIR) -o libqthread.so $(SRC_DIR)/libqthread.cpp $(DYNAMIC_LIB)
	mv libqthread.so ./lib


libqthread.a: qthread.o
	mkdir -p lib
	ar rsv libqthread.a qthread.o
	mv libqthread.a ./lib
 

qthread.o:
	$(CC) $(CFLAGS) -I$(INC_DIR) -o qthread.o -c $(SRC_DIR)/libqthread.cpp 

clean:
	rm -rf ./lib *.o
