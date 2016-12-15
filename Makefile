all: meminfo

#which comipler
CC = gcc

#where are include files kept
INCLUDE = .

#options for development
CFLAGS = -g #-Wall

#CFLAGS = -DANDROID

meminfo: main.o error.o getmem.o getpss.o hash.o
		$(CC) $(CFLAGS) -o meminfo main.o getmem.o error.o getpss.o hash.o

main.o: main.c
		$(CC) $(CFLAGS) -c main.c

getmem.o: getmem.c getmem.h
		$(CC) $(CFLAGS) -c getmem.c

error.o: error.c error.h
		$(CC) $(CFLAGS) -c error.c

getpss.o: getpss.c getpss.h
		$(CC) $(CFLAGS) -c getpss.c

hash.o: hash.c hash.h
		$(CC) $(CFLAGS) -c hash.c

clean:
		-rm *.o
		-rm meminfo
