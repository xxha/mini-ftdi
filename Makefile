
LIB_NAME = libftdi
LIB_LINK_NAME = $(LIB_NAME).so
LINKLIB = -Wl,-rpath,./ $(LIB_LINK_NAME)
CFLAGS = -Wall -O2

CC=gcc
all: ux400gps
ux400gps: ux400gps.c
	$(CC) $^ -o $@ $(LINKLIB) 

clean:
	rm -f *.o

