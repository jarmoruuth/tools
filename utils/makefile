######################################################################
#
#	Makefile for lj, rj and calcj in Linux and other POSIX
#	systems. In Windows use: nmake -f makefile.win
#
#	make		builds lj, rj and calcj
#	make test	runs test.sh
#
######################################################################

CC	= cc
CFLAGS	= -O2 -Wall

all: lj rj calcj

lj: lj.o pdir.o
	$(CC) $(CFLAGS) -o lj lj.o pdir.o

rj: rj.o pdir.o
	$(CC) $(CFLAGS) -o rj rj.o pdir.o

calcj: calcj.o pdir.o
	$(CC) $(CFLAGS) -o calcj calcj.o pdir.o -lm

lj.o rj.o calcj.o pdir.o: pdir.h

.c.o:
	$(CC) $(CFLAGS) -c $<

test: lj rj
	bash test.sh ./lj ./rj

clean:
	rm -f *.o lj rj calcj

.PHONY: all test clean
