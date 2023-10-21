CC = g++
DEBUG = -g -Wall
STD = c++11
OBJS = main_server.o server.o parse.o socket_epoll.o log.o config.o
OBJC = main_client.o client.o parse.o log.o config.o
DEP_INCLUDE_PATH= -I ./

ALL: server client

server: $(OBJS)
	$(CC) -std=$(STD) $^ -o $@ $(DEBUG) -lpthread -lsqlite3

client: $(OBJC)
	$(CC) -std=$(STD) $^ -o $@ $(DEBUG) -lpthread

%.o: %.cpp
	$(CC) -std=$(STD) $(DEP_INCLUDE_PATH) -c $< $(DEBUG) -o $@ -lpthread -lsqlite3


.PHONY:
clean:
	@rm -f *.o *.h.gch client server
 
