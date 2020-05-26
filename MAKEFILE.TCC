# ::[[ @(#) makefile.tcc 1.1 89/07/02 00:19:54 ]]::
# Makefile for flip for MS-DOS and Turbo C 2.0.  The supplied turboc.cfg
# configuration file must be in the current directory;  edit it to make
# sure the include directories are correct.

CC = tcc
CFLAGS = -c -DTURBOC -DLINT -DNDEBUG
CFMORE =
LD = tcc
LDFLAGS = -eFLIP
LDMORE =

OBJS = flip.obj getopt.obj turboc.obj

flip.exe: $(OBJS)
	$(LD) $(LDFLAGS) $(LDMORE) $(OBJS)

flip.obj: flip.c flip.h
	$(CC) $(CFLAGS) $(CFMORE) $*.c

getopt.obj: getopt.c flip.h
	$(CC) $(CFLAGS) $(CFMORE) $*.c

turboc.obj: turboc.c flip.h
	$(CC) $(CFLAGS) $(CFMORE) $*.c
