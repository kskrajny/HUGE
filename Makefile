CC = gcc
CFLAGS = -pthread -Wall -Wextra -O2
TARGETS = radio-proxy multi-send

all: $(TARGETS)

err.o: err.c err.h

input.o: input.c input.h err.h

multi-send.o: multi-send.c err.h

radio-proxy.o: radio-proxy.c input.h err.h 

multi-send: multi-send.o err.o
	$(CC) $(CFLAGS) $^ -o $@

radio-proxy: radio-proxy.o input.o err.o
	$(CC) $(CFLAGS) $^ -o $@ -levent

clean:
	rm -f *.o $(TARGETS) 