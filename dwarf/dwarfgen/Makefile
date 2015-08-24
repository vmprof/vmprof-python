#
# Makefile.in uses very simple make rules.
# There are no restrictions on copying this file.
#

# Standard Makefile.in stuff:
srcdir =        .


prefix =        /usr/local
exec_prefix =   ${prefix}
bindir =        $(exec_prefix)/bin
libdir =        $(exec_prefix)/lib
mandir =        $(exec_prefix)/share/man
man1dir =       $(mandir)/man1

INSTALL =       /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA =  ${INSTALL} -m 644
DATAROOT =      ${prefix}/share
SHELL =         /bin/sh
CXX =            g++
AR =            ar
#ARFLAGS =      @ARFLAGS@
RM =            rm
RANLIB =        ranlib
DEFS =          -DHAVE_CONFIG_H
LIBS =           -L ../libdwarf -ldwarf -lelf
INCLUDES =      -I. -I$(srcdir) -I$(srcdir)/../libdwarf
CXXFLAGS =        -g -O2 $(INCLUDES) 
LDFLAGS =        $(LIBS)


DIRINC = $(srcdir)/../libdwarf
INSTALL = cp

binprefix =

DGOBJECTS = \
  createirepformfrombinary.o \
  createirepfrombinary.o \
  dwarfgen.o  \
  irepattrtodbg.o \
  ireptodbg.o

HEADERS = \
  createirepfrombinary.h\
  general.h \
  irepattrtodbg.h \
  irepdie.h \
  irepframe.h \
  irepform.h \
  irepmacro.h \
  irepresentation.h \
  ireptodbg.h \
  strtabdata.h 

all: dwarfgen

default: $(TARGETS)

# We reach-around to libdwarf to get the dwgetopt src, header.
# We could equally well reach around to dwarfdump.
dwgetopt.o: $(srcdir)/../libdwarf/dwgetopt.c  $(srcdir)/../libdwarf/dwgetopt.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(srcdir)/../libdwarf/dwgetopt.c
dwarfgen: $(HEADERS) $(DGOBJECTS) dwgetopt.o
	$(CXX) $(CXXFLAGS) -o $@ $(DGOBJECTS) $(LDFLAGS) dwgetopt.o

test:

install: all
	$(INSTALL) $(srcdir)/dwarfgen.conf $(libdir)/dwarfgen.conf
	$(INSTALL) $(srcdir)/dwarfgen.1 $(man1dir)/dwarfgen.1
	$(INSTALL) dwarfgen $(bindir)/dwarfgen

clean:
	-rm -f *.o dwarfgen

uninstall:
	-rm -f $(libdir)/dwarfgen.conf
	-rm -f $(man1dir)/dwarfgen.1
	-rm -f $(bindir)/dwarfgen

distclean: clean
	-rm -f config.log config.h config.cache config.status 
	rm -rf autom4te.cache	
	rm -f Makefile

shar:
	@echo "shar not set up."

dist:
	@echo "dist not set up."
