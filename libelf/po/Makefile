# Generated automatically from Makefile.in by configure.
# po/Makefile for libelf.
# Copyright (C) 1995 - 2006 Michael Riepe
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# @(#) $Id: Makefile.in,v 1.17 2006/04/21 17:16:51 michael Exp $

instroot =

prefix = /usr/local
exec_prefix = ${prefix}
localedir = $(prefix)/lib/locale

CC = gcc
RM = rm -f
MV = mv -f
INSTALL = /usr/bin/install -c
INSTALL_DATA = ${INSTALL} -m 644

GENCAT = /usr/bin/gencat
GMSGFMT = msgfmt
MSGFMT = no
XGETTEXT = xgettext
MSGMERGE = 

CFLAGS = -g -O2
CPPFLAGS = 
DEFS = -DHAVE_CONFIG_H
LDFLAGS = 
LIBS = 
LIBINTL = 

# no user serviceable parts below

PACKAGE = libelf
VERSION = 0.8.9

SHELL = /bin/sh


srcdir = .
top_srcdir = ..

topdir = ..
subdir = po

.SUFFIXES:
.SUFFIXES: .po .mo .gmo .msg .cat

.po.mo:
	@$(RM) $@
	$(MSGFMT) -o $@ $<

.po.gmo:
	file=$(srcdir)/`echo $*|sed 's,.*/,,'`.gmo; \
	  $(RM) $$file && $(GMSGFMT) -o $$file $<

.msg.cat:
	@$(RM) $@
	$(GENCAT) $@ $<

POFILES =  de.po
GMOFILES =  de.gmo
MSGFILES =  de.msg

DISTFILES = \
	gmo2msg.c Makefile.in $(PACKAGE).pot stamp-po \
	$(POFILES) $(GMOFILES) $(MSGFILES)

POTFILES = $(top_srcdir)/lib/errors.h

CATALOGS =  de.cat
CATOBJEXT = .cat
INSTOBJEXT = .cat

all: $(CATALOGS)

check:

install: all install-data

install-data: $(top_srcdir)/mkinstalldirs
	catalogs="$(CATALOGS)"; for cat in $$catalogs; do \
	  lang=`echo $$cat | sed 's,$(CATOBJEXT)$$,,'`; \
	  dir=$(localedir)/$$lang/LC_MESSAGES; \
	  $(SHELL) $(top_srcdir)/mkinstalldirs $(instroot)$$dir; \
	  if test -r $$cat; then \
	    $(INSTALL_DATA) $$cat $(instroot)$$dir/$(PACKAGE)$(INSTOBJEXT); \
	  else \
	    $(INSTALL_DATA) $(srcdir)/$$cat $(instroot)$$dir/$(PACKAGE)$(INSTOBJEXT); \
	  fi; \
	done

uninstall:
	catalogs="$(CATALOGS)"; for cat in $$catalogs; do \
	  lang=`echo $$cat | sed 's,$(CATOBJEXT)$$,,'`; \
	  $(RM) $(instroot)$(localedir)/$$lang/LC_MESSAGES/$(PACKAGE)$(INSTOBJEXT); \
	done

mostlyclean:
	$(RM) core core.* $(PACKAGE).po *.po.tmp

clean: mostlyclean

distclean: clean
	$(RM) gmo2msg *.mo *.cat
	$(RM) Makefile

maintainer-clean: distclean
	$(RM) stamp-po

$(PACKAGE).pot: $(POTFILES)
	$(XGETTEXT) -c -d$(PACKAGE) -k_ $(POTFILES)
	if cmp -s $(PACKAGE).po $(srcdir)/$(PACKAGE).pot; then \
	  $(RM) $(PACKAGE).po; \
	else \
	  $(RM) $(srcdir)/$(PACKAGE).pot && \
	    $(MV) $(PACKAGE).po $(srcdir)/$(PACKAGE).pot; \
	fi

update-po: stamp-po
stamp-po: $(PACKAGE).pot
	pofiles="$(POFILES)"; cd $(srcdir) && for po in $$pofiles; do \
	  $(RM) $$po.tmp; \
	  if $(MSGMERGE) $$po $(PACKAGE).pot > $$po.tmp; then \
	    $(RM) $$po; \
	    $(MV) $$po.tmp $$po; \
	  else \
	    echo "update for $$po failed!"; \
	    $(RM) $$po.tmp; \
	  fi; \
	done
	$(RM) $@ && echo timestamp > $@

# Create X/Open message catalog sources from .gmo files.

.gmo.msg:
	$(MAKE) $(srcdir)/gmo2msg
	cd $(srcdir) && ./gmo2msg `echo $*|sed 's,.*/,,'`

.SUFFIXES: .c

.c:
	@$(RM) $@
	$(CC) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) $(XCFLAGS) \
	  $(LDFLAGS) $*.c $(LIBS) $(LIBINTL) -o $@

INCLUDES = -I$(topdir) -I. -I$(topdir)/lib -I$(srcdir) -I$(top_srcdir)/lib

# maintainer only

MAINT = maintainer-only-

distdir = $(PACKAGE)-$(VERSION)
distsubdir = $(topdir)/$(distdir)/$(subdir)
$(MAINT)dist: update-po $(DISTFILES)
	if test -d $(distsubdir); then true; else mkdir $(distsubdir); fi
	files="$(DISTFILES)"; for file in $$files; do \
	  ln $(srcdir)/$$file $(distsubdir) >/dev/null 2>&1 || \
	    cp -p $(srcdir)/$$file $(distsubdir) || exit 1; \
	done

# For the justification of the following Makefile rules, see node
# `Automatic Remaking' in GNU Autoconf documentation.

$(MAINT)Makefile: Makefile.in $(topdir)/config.status
	cd $(topdir) && CONFIG_FILES=$(subdir)/$@ CONFIG_HEADERS= ./config.status

# Tell versions [3.59,3.63) of GNU make not to export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
