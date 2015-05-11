#*****************************************************************************
#
#	Makefile for library utilities
#
#*****************************************************************************

#*****************************************************************************
#
# Object file name extensions
#
#       D - Dos
#       O - OS/2
#       N - NT
#
#       S - Small model
#       C - Compact model
#       L - Large model
#
#       O - Object file
#
# Library file names
#
#       C - C library file prefix
#
#       S - Small model
#       C - Compact model
#       L - Large model
#       
#       DOS - Dos library
#       OS2 - OS/2 library
#       WNT - NT library
#
#*****************************************************************************

.SUFFIXES: .lib .nto .dso .dlo .oso .olo .c .h

NT=Y

#*****************************************************************************

LIBDIR = ..\lib
INCDIR = ..\include

CC     = cl
CFLAGS = -W3 -Zp -Gs -Zl -I.

LIBMAN = gmakelib lib

#*****************************************************************************

ifdef DOS
default: csdos.lib cldos.lib
SC = ;
endif

ifdef OS2
default: csos2.lib clos2.lib
SC = ;
endif

ifdef NT
default: cwnt.lib
LIBMAN = gmakelib lib32
endif

#*****************************************************************************

.c.dso:
	$(CC) -c -DDOS -DMSC -AS $(CFLAGS) -Fo$*.dso  $*.c

.c.dlo:
	$(CC) -c -DDOS -DMSC -AL $(CFLAGS) -Fo$*.dlo  $*.c

.c.oso:
	$(CC) -c -DOS2 -DMSC -AS $(CFLAGS) -Fo$*.oso  $*.c

.c.olo:
	$(CC) -c -DOS2 -DMSC -AL $(CFLAGS) -Fo$*.olo  $*.c

.c.nto:
	$(CC) -c -Ox -GL -DNT -DMSC $(CFLAGS) -Fo$*.nto  $*.c

#*****************************************************************************

csdos.lib: dosvio.dso kbd.dso diruti.dso ectype.dso filelist.dso getopt.dso\
	   listmenu.dso timer.dso diskfree.dso splitpat.dso dirutior.dso
	- del $*.lib
	$(LIBMAN) $*.lib $^ $(SC)

cldos.lib: dosvio.dlo kbd.dlo diruti.dlo ectype.dlo filelist.dlo getopt.dlo\
	   listmenu.dlo timer.dlo diskfree.dlo splitpat.dlo dirutior.dlo
	- del $*.lib
	$(LIBMAN) $*.lib $^ $(SC)

csos2.lib: os2vio.oso kbd.oso diruti.oso ectype.oso filelist.oso getopt.oso\
	   listmenu.oso timer.oso diskfree.oso splitpat.oso dirutior.oso
	- del $*.lib
	$(LIBMAN) $*.lib $^ $(SC)

clos2.lib: os2vio.olo kbd.olo diruti.olo ectype.olo filelist.olo getopt.olo\
	   listmenu.olo timer.olo diskfree.olo splitpat.olo dirutior.olo
	- del $*.lib
	$(LIBMAN) $*.lib $^ $(SC)

cwnt.lib: diruti.nto ectype.nto filelist.nto getopt.nto\
	 timer.nto diskfree.nto splitpat.nto dirutior.nto
	- del $*.lib
	$(LIBMAN) $*.lib $^ $(SC)

#*****************************************************************************

h: $(INCDIR)\filelist.h $(INCDIR)\listmenu.h $(INCDIR)\vio.h \
   $(INCDIR)\diruti.h $(INCDIR)\getopt.h $(INCDIR)\timer.h \
   $(INCDIR)\ectype.h $(INCDIR)\kbd.h $(INCDIR)\c.h

$(INCDIR)\filelist.h: filelist.h
	copy filelist.h $(INCDIR)\filelist.h

$(INCDIR)\listmenu.h: listmenu.h
	copy listmenu.h $(INCDIR)\listmenu.h

$(INCDIR)\vio.h: vio.h
	copy vio.h $(INCDIR)\vio.h

$(INCDIR)\diruti.h: diruti.h
	copy diruti.h $(INCDIR)\diruti.h

$(INCDIR)\getopt.h: getopt.h
	copy getopt.h $(INCDIR)\getopt.h

$(INCDIR)\timer.h: timer.h
	copy timer.h $(INCDIR)\timer.h

$(INCDIR)\ectype.h: ectype.h
	copy ectype.h $(INCDIR)\ectype.h

$(INCDIR)\kbd.h: kbd.h
	copy kbd.h $(INCDIR)\kbd.h

$(INCDIR)\c.h: c.h
	copy c.h $(INCDIR)\c.h
