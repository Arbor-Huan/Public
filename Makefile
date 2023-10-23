CC = g++
DEBUG = -g -Wall
STD = c++11
INCLUDE_PATH = -I ./include

SRC_DIR = ./src
OBJ_DIR = ./obj

_OBJS = main_server.o server.o parse.o socket_epoll.o log.o config.o threadpool.o
OBJS = $(patsubst %,$(OBJ_DIR)/%,$(_OBJS))

_OBJC = main_client.o client.o parse.o log.o config.o
OBJC = $(patsubst %,$(OBJ_DIR)/%,$(_OBJC))

ALL: server client

server: $(OBJS)
	$(CC) -std=$(STD) $^ -o $@ $(DEBUG) -lpthread -lsqlite3

client: $(OBJC)
	$(CC) -std=$(STD) $^ -o $@ $(DEBUG) -lpthread

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CC) -std=$(STD) $(INCLUDE_PATH) -c $< $(DEBUG) -o $@ -lpthread -lsqlite3

.PHONY: clean
clean:
	rm -f $(OBJ_DIR)/*.o *.h.gch ./server ./client