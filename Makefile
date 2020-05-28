CC = gcc
CFLAGS = -pthread -Wall -Wextra -O2
TARGETS = radio-proxy

all: $(TARGETS)

err.o: err.c err.h

input.o: input.c input.h err.h

radio-proxy.o: radio-proxy.c input.h err.h 

radio-proxy: radio-proxy.o input.o err.o
	$(CC) $(CFLAGS) $^ -o $@ -levent

clean:
	rm -f *.o $(TARGETS) 
