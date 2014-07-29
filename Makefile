

CC = g++
CFLAGS = -m64 -O3 -DDEBUG -shared -fPIC -g

all: libqthread.so 

INC_DIR = ./include
SRC_DIR = ./src


libqthread.so: 
	mkdir -p lib
	$(CC) $(CFLAGS) -I$(INC_DIR) -o libqthread.so $(SRC_DIR)/libqthread.cpp -ldl -lpthread
	mv libqthread.so ./lib
 

clean:
	rm -rf ./lib
