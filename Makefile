
LIB_NAME = libftdi
LIB_LINK_NAME = $(LIB_NAME).so
LINKLIB = -Wl,-rpath,./ $(LIB_LINK_NAME)
CFLAGS = -Wall -O2

CC=gcc
all: ux400gps ux400opm ux400aclock ux400opmtest ux400setclk

ux400gps: ux400gps.c
	$(CC)  ux400gps.c -o ux400gps $(LINKLIB) 

ux400opm: ux400opm.c
	$(CC)  ux400opm.c -o ux400opm -pthread -L libftdi.la $(LINKLIB) 

ux400aclock: ux400aclock.c
	$(CC)  ux400aclock.c -o ux400aclock -pthread -L libftdi.la $(LINKLIB) 

ux400opmtest: ux400opmtest.c
	$(CC)  ux400opmtest.c -o ux400opmtest -pthread -L libftdi.la $(LINKLIB) 

ux400setclk: ux400setclk.c
	$(CC)  ux400setclk.c -o ux400setclk -pthread -L libftdi.la $(LINKLIB) 

clean:
	rm -f *.o ux400gps ux400opm ux400aclock ux400opmtest

