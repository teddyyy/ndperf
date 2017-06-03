CC = gcc

OBJ = ndperf.o flow.o counter.o
CFLAGS = -Wall -g

ndperf : $(OBJ)
	$(CC) -o ndperf $(OBJ)
	rm $(OBJ)

.c.o:
	$(CC) $(CFLAGS) -c $*.c

clean :
	rm ndperf
