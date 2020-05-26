# Makefile for flip for **IX.  ::[[ @(#) makefile.nix 1.4 89/07/04 12:00:02 ]]::

# The contents of this makefile are hereby released to the public domain.
#                               -- Rahul Dhesi 1989/06/19

PACKAGE = flip
VERSION = 1.21
distdir = $(PACKAGE)-$(VERSION)
srcdir = .
TAR = tar
GZIP_ENV = --best
DISTFILES=FILES INSTALL.DOC MAKEFILE.TCC test-flip	\
 MESSAGE Makefile TURBOC.C TURBOC.CFG flip.1 flip.c flip.h getopt.c ChangeLog

# Before use make sure this file is called "makefile".  (Rename it if
# necessary.)  Then invoke as follows:

# To build on a modern Unix system with a standard C compiler:
#   make "CFLAGS= -DBSD -DIX -DSTDINCLUDE" flip
# Otherwise:
#   "make sys_v"        makes executable flip for System V Release 2
#   "make bsd"          makes executable flip for 4.3BSD

#   "make install"      moves flip into BINDIR, copies flip.1 into MANDIR
#   "make clean"        deletes object and executable files

# Where to install executable and manual files on "make install".  The trailing
# "/." forces an error message if the destination directory doesn't exist.
BINDIR = /usr/local/bin/.
MANDIR = /usr/man/man1/.

# CC is compiler, LD is loader (may be same as compiler), CFLAGS are flags
# for compiler, LDFLAGS are flags for loader, CFMORE are additional
# (relatively unchanging) flags for compiler

CC = cc
CFLAGS =
CFMORE = -c -DNDEBUG -O -DVERSION=\"$(VERSION)\"
LD = cc
LDFLAGS = -o flip
RM = rm -f

# Work out the platform, including Windows
ifeq '$(findstring ;,$(PATH))' ';'
	UNAME := $(shell uname 2>NUL || echo Windows)
else
	UNAME := $(shell uname 2>/dev/null || echo Unknown)
	UNAME := $(patsubst CYGWIN%,Cygwin,$(UNAME))
	UNAME := $(patsubst MSYS%,MSYS,$(UNAME))
	UNAME := $(patsubst MINGW%,MSYS,$(UNAME))
endif

# Platform specific differences here
ifeq ($(UNAME),Windows)
	CC = gcc
	LD = gcc
	RM = del
	XTRA_OP = flip.exe
endif

# If your system does not supply getopt as a library function,
# add getopt.o to the RHS list on the next line and uncomment the
# two nonblank lines after that.
OBJS = flip.o

#getopt.o: getopt.c flip.h
#	$(CC) $(CFLAGS) $(CFMORE) $*.c

nothing:
	@echo \
	'Please type "make sys_v", "make bsd", "make uport", "make ultrix", or "make mingw"'

sys_v:
	make "CFLAGS=-DSYS_V -DIX" flip

uport:
	make "CFLAGS=-DSYS_V -Ml -DIX" "LDFLAGS=-Ml -o flip" flip

bsd:
	make "CFLAGS=-DBSD -DIX" flip

ultrix:
	make "CFLAGS=-DBSD -DULTRIX_BUG" flip

mingw:
	make "CFLAGS=-DMINGW" flip

flip: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS)

flip.o: flip.c flip.h
	$(CC) $(CFLAGS) $(CFMORE) $*.c

clean:
	$(RM) *.o core flip $(XTRA_OP)

ifeq ($(UNAME),Windows)
install: install-win
else
install: install-default
endif

install-win:
	copy flip.exe c:\windows\system32

install-default:
	mv flip $(BINDIR)
	cp flip.1 $(MANDIR)


check:
	bash test-flip

dist: $(DISTFILES)
	-rm -rf $(distdir)
	mkdir $(distdir)
	-chmod 777 $(distdir)
	@for file in $(DISTFILES); do \
	  d=$(srcdir); \
	  if test -d $$d/$$file; then \
	    cp -pr $$d/$$file $(distdir)/$$file; \
	  else \
	    test -f $(distdir)/$$file \
	    || ln $$d/$$file $(distdir)/$$file 2> /dev/null \
	    || cp -p $$d/$$file $(distdir)/$$file || :; \
	  fi; \
	done
	-chmod -R a+r $(distdir)
	GZIP=$(GZIP_ENV) $(TAR) -chozf $(distdir).tar.gz $(distdir)
	-rm -rf $(distdir)
