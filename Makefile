

CC = g++
CFLAGS = -m64 -O3 -DDEBUG -shared -fPIC 

all: libqthread.so

INC_DIR = ./src/inc
SRC_DIR = ./src


libqthread.so: 
	mkdir -p lib
	$(CC) $(CFLAGS) -I$(INC_DIR) -o libqthread.so $(SRC_DIR)/wrapper.cpp -ldl -lpthread
	mv libqthread.so lib


clean:
	rm -f lib/libqthread.so
