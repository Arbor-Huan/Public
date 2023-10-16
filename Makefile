CC = g++
DEBUG = -g -Wall
STD = c++11
OBJS = server.o parse.o socket_epoll.o log.o config.o
OBJC = client.o parse.o log.o config.o
DEP_INCLUDE_PATH= -I ./

ALL: server client

server: $(OBJS)
	$(CC) -std=$(STD) $^ -o $@ $(DEBUG) -lpthread

client: $(OBJC)
	$(CC) -std=$(STD) $^ -o $@ $(DEBUG) -lpthread

%.o: %.cpp
	$(CC) -std=$(STD) $(DEP_INCLUDE_PATH) -c $< $(DEBUG) -o $@ -lpthread


.PHONY:
clean:
	@rm -f *.o *.h.gch client server
 
