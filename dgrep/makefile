######################################################################
#
#	Makefile for dgrep and test programs in Linux and other POSIX
#	systems. In Windows use: nmake -f makefile.win
#
#	make		builds dgrep and try
#	make test	builds also dgrep-smallbuf and runs regress.sh
#	make bench	runs bench.sh
#
######################################################################

CC	= cc
CFLAGS	= -O2 -Wall

DFASRC	= dfaregex.c dfa.c calcpos.c dfatree.c regmust.c set.c clib.c getopt.c
DGREPSRC = dgrep.c bm.c $(DFASRC)
HEADERS	= system.h dfa.h set.h dfaregex.h bm.h getopt.h

DFAOBJ	= dfaregex.o dfa.o calcpos.o dfatree.o regmust.o set.o clib.o getopt.o
DGREPOBJ = dgrep.o bm.o $(DFAOBJ)
TRYOBJ	= try.o $(DFAOBJ)

all: dgrep try

dgrep: $(DGREPOBJ)
	$(CC) $(CFLAGS) -o dgrep $(DGREPOBJ)

try: $(TRYOBJ)
	$(CC) $(CFLAGS) -o try $(TRYOBJ)

# dgrep with a tiny input buffer for testing buffer boundaries
dgrep-smallbuf: $(DGREPSRC) $(HEADERS)
	$(CC) $(CFLAGS) -DMAXBUF=256 -o dgrep-smallbuf $(DGREPSRC)

$(DGREPOBJ) try.o: $(HEADERS)

.c.o:
	$(CC) $(CFLAGS) -c $<

test: dgrep try dgrep-smallbuf
	bash regress.sh ./dgrep ./try ./dgrep-smallbuf

bench: dgrep
	bash bench.sh ./dgrep

clean:
	rm -f *.o dgrep try dgrep-smallbuf

.PHONY: all test bench clean
