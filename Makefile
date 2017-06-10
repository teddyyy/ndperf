CC = gcc

OBJ = ndperf.o flow.o counter.o interface.o
CFLAGS = -Wall -g
INCPATH = /usr/include/libnl3/
LDFLAGS = -lnl-3 -lnl-route-3 -lpthread

ndperf : $(OBJ)
	$(CC) -o ndperf -I$(INCPATH) $(OBJ) $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) -I$(INCPATH) -c $*.c

.PHONY: clean
clean :
	rm ndperf $(OBJ)
