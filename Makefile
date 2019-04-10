# makefile for zmakebas

# comment out the `-DHAVE_GETOPT' if, for some reason or other, you
# don't have getopt(). (This is mainly so it'll work on MS-DOG, though
# I'm not entirely sure why I bothered supporting that. :-))
#
CC=gcc
CFLAGS=-O -Wall -DHAVE_GETOPT

# these set where the executable and man page are installed
PREFIX=/usr/local

BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/man/man1


all: zmakebas

zmakebas: zmakebas.o
	$(CC) $(CFLAGS) -o zmakebas zmakebas.o

installdirs:
	/bin/sh ./mkinstalldirs $(BINDIR) $(MANDIR)

install: zmakebas installdirs
	install -m 511 zmakebas $(BINDIR)
	install -m 444 zmakebas.1 $(MANDIR)

uninstall:
	$(RM) $(BINDIR)/zmakebas
	$(RM) $(MANDIR)/zmakebas.1*

clean:
	$(RM) *~ *.o zmakebas


# The stuff below makes the distribution tgz.

VERS=1.3

dist: tgz
tgz: ../zmakebas-$(VERS).tar.gz
  
../zmakebas-$(VERS).tar.gz: clean
	$(RM) ../zmakebas-$(VERS)
	@cd ..;ln -s zmakebas zmakebas-$(VERS)
	cd ..;tar zchvf zmakebas-$(VERS).tar.gz zmakebas-$(VERS)
	@cd ..;$(RM) zmakebas-$(VERS)
