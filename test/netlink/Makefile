INC_PATH = /usr/include/libnl3/
LDFLAGS = -lnl-3 -lnl-route-3
CFLAGS = -Wall -g

all: create-macvlan delete-macvlan

create-macvlan: create-macvlan.o
	gcc -o create-macvlan -I$(INC_PATH) create-macvlan.o $(LDFLAGS)
	rm create-macvlan.o

delete-macvlan: delete-macvlan.o
	gcc -o delete-macvlan -I$(INC_PATH) delete-macvlan.o $(LDFLAGS)
	rm delete-macvlan.o

.c.o:
	gcc $(CFLAGS) -I$(INC_PATH) -c $*.c

clean:
	rm create-macvlan delete-macvlan
