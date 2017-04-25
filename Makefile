CC = gcc

objects = ndperf.o
CFLAGS = -Wall -g

ndperf : $(objects)
	$(CC) -o ndperf $(objects)

.c.o:
	$(CC) $(CFLAGS) -c $*.c

clean :
	rm ndperf $(objects)
