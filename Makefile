CC = gcc
CFLAGS = -O2 -Wall -I .

# This flag includes the Pthreads library on a Linux box.
# Others systems will probably require something different.
LIB = -lpthread

all: tiny

tiny: tiny.c csapp.o
	$(CC) $(CFLAGS) -o tiny csapp.c cache.c tiny.c $(LIB)

#$csapp.o:
#	$(CC) $(CFLAGS) -c csapp.c


