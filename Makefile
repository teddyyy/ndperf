CC = gcc

OBJ = ndperf.o flow.o counter.o
CFLAGS = -Wall -g

ndperf : $(OBJ)
	$(CC) -o ndperf $(OBJ)

.c.o:
	$(CC) $(CFLAGS) -c $*.c

.PHONY: clean
clean :
	rm ndperf $(OBJ)
