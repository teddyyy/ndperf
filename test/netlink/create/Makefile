objects = create-macvlan.o
INC_PATH = /usr/include/libnl3/
LDFLAGS = -lnl-3 -lnl-route-3
CFLAGS = -Wall -g

create-macvlan: $(objects)
	gcc -o create-macvlan -I$(INC_PATH) $(objects) $(LDFLAGS)
	rm $(objects)

.c.o:
	gcc $(CFLAGS) -I$(INC_PATH) -c $*.c
