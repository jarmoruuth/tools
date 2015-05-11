######################################################################
#
#	Makefile for dgrep and test programs in Windows NT
#
######################################################################

dgrep:	dgrep.exe
try:	try.exe

WILDARG	= "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\lib\amd64\setargv.obj"

CC	= cl
CFLAGS	= -Ox -GL -c -DECTYPE -W3 -DNT -DMSC -I..\lib

LINK	= cl
LIBS	= -link ..\lib\cwnt.lib

DFAOBJ	= dfaregex.obj dfa.obj calcpos.obj dfatree.obj regmust.obj set.obj clib.obj
	  
DGREPOBJ = dgrep.obj bm.obj $(DFAOBJ)

dgrep.exe: $(DGREPOBJ)
	$(LINK) $(LFLAGS) $(DGREPOBJ) $(WILDARG) $(LIBS)

TRYOBJ = try.obj $(DFAOBJ)

try.exe: $(TRYOBJ)
	$(LINK) $(LFLAGS) $(TRYOBJ) $(LIBS)

.c.obj:
	$(CC) $(CFLAGS) $*.c

dfa.h:		set.h

dgrep.obj:	dgrep.c dfaregex.h dfa.h system.h
	$(CC) $(CFLAGS) dgrep.c
	
bm.obj:		bm.c dfa.h system.h
	$(CC) $(CFLAGS) $*.c
	
dfaregex.obj:	dfaregex.c dfaregex.h dfa.h system.h
dfa.obj:	dfa.c dfa.h system.h
calcpos.obj:	calcpos.c dfa.h system.h
dfatree.obj:	dfatree.c dfa.h system.h
regmust.obj:	regmust.c dfa.h system.h
set.obj:	set.c set.h system.h
clib.obj:	clib.c system.h
try.obj:	try.c dfaregex.h

