# Makefile for gpscorrelate
# Written by Daniel Foote.

PACKAGE_VERSION = 1.6.2git

CC = gcc
CXX = g++
EXEEXT =
PKG_CONFIG=pkg-config

COBJS    = main-command.o unixtime.o gpx-read.o correlate.o exif-gps.o latlong.o
GOBJS    = main-gui.o gui.o unixtime.o gpx-read.o correlate.o exif-gps.o latlong.o
CFLAGS   = -Wall -O2
CFLAGSINC := $(shell $(PKG_CONFIG) --cflags libxml-2.0 exiv2)
# Add the gtk+ flags only when building the GUI
GTK      = 2
gpscorrelate-gui$(EXEEXT): CFLAGSINC += $(shell $(PKG_CONFIG) --cflags gtk+-$(GTK).0)
LIBS :=
LDFLAGS   = -Wall -O2
LDFLAGSALL := $(shell $(PKG_CONFIG) --libs libxml-2.0 exiv2) -lm
LDFLAGSGUI := $(shell $(PKG_CONFIG) --libs gtk+-$(GTK).0)

# Put --nonet here to avoid downloading DTDs while building documentation
XSLTFLAGS =

prefix   = /usr/local
bindir   = $(prefix)/bin
datadir  = $(prefix)/share
mandir   = $(datadir)/man
docdir   = $(datadir)/doc/gpscorrelate
applicationsdir = $(datadir)/applications

DEFS = -DPACKAGE_VERSION=\"$(PACKAGE_VERSION)\"

TARGETS = gpscorrelate-gui$(EXEEXT) gpscorrelate$(EXEEXT) doc/gpscorrelate.1 doc/gpscorrelate.html

all:	$(TARGETS)

gpscorrelate$(EXEEXT): $(COBJS)
	$(CXX) -o $@ $(COBJS) $(LDFLAGS) $(LDFLAGSALL) $(LIBS)

gpscorrelate-gui$(EXEEXT): $(GOBJS)
	$(CXX) -o $@ $(GOBJS) $(LDFLAGS) $(LDFLAGSGUI) $(LDFLAGSALL) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(CFLAGSINC) $(DEFS) -c -o $@ $<

.cpp.o:
	$(CXX) $(CFLAGS) $(CFLAGSINC) $(DEFS) -c -o $@ $<

# Hack to recompile everything if a header changes
*.o: *.h

check: gpscorrelate$(EXEEXT)
	(cd tests && ./testsuite)

clean:
	rm -f *.o gpscorrelate$(EXEEXT) gpscorrelate-gui$(EXEEXT) doc/gpscorrelate-manpage.xml tests/log/* $(TARGETS)

install: all
	install -d $(DESTDIR)$(bindir)
	install gpscorrelate$(EXEEXT) gpscorrelate-gui$(EXEEXT) $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(mandir)/man1
	install -m 0644 doc/gpscorrelate.1 $(DESTDIR)$(mandir)/man1
	install -d $(DESTDIR)$(docdir)
	install -p -m 0644 doc/*.html doc/*.png README.md $(DESTDIR)$(docdir)
	install -d $(DESTDIR)$(docdir)/fr
	install -p -m 0644 doc/fr/*.html $(DESTDIR)$(docdir)/fr

install-desktop-file:
	desktop-file-install --vendor="" --dir="$(DESTDIR)$(applicationsdir)" gpscorrelate.desktop
	install -d $(DESTDIR)$(datadir)/icons/hicolor/scalable/apps
	install -p -m 0644 gpscorrelate-gui.svg $(DESTDIR)$(datadir)/icons/hicolor/scalable/apps/gpscorrelate-gui.svg

docs: doc/gpscorrelate.1  doc/gpscorrelate.html

doc/gpscorrelate-manpage.xml: doc/gpscorrelate-manpage.xml.in
	sed -e 's,@DOCDIR@,$(docdir),' -e 's,@PACKAGE_VERSION@,$(PACKAGE_VERSION),' $< > $@

doc/gpscorrelate.1: doc/gpscorrelate-manpage.xml
	xsltproc $(XSLTFLAGS) -o $@ http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

doc/gpscorrelate.html: doc/gpscorrelate-manpage.xml
	xsltproc $(XSLTFLAGS) -o $@ http://docbook.sourceforge.net/release/xsl/current/html/docbook.xsl $<

build-po:
	(cd po && $(MAKE) VERSION="$(PACKAGE_VERSION)" prefix="$(prefix)" top_srcdir="$(PWD)" update-po)
	(cd po && $(MAKE) VERSION="$(PACKAGE_VERSION)" prefix="$(prefix)" top_srcdir="$(PWD)" all)

install-po: build-po
	(cd po && $(MAKE) VERSION="$(PACKAGE_VERSION)" prefix="$(prefix)" top_srcdir="$(PWD)" install)

clean-po:
	(cd po && $(MAKE) VERSION="$(PACKAGE_VERSION)" prefix="$(prefix)" top_srcdir="$(PWD)" clean)

dist:
	mkdir gpscorrelate-$(PACKAGE_VERSION)
	git archive --prefix=gpscorrelate-$(PACKAGE_VERSION)/ HEAD | tar xf -
	-rm gpscorrelate-$(PACKAGE_VERSION)/po/stamp-po
	cd gpscorrelate-$(PACKAGE_VERSION)/po && $(MAKE) gpscorrelate.pot-update clean
	-rm gpscorrelate-$(PACKAGE_VERSION)/po/stamp-po
	tar cf gpscorrelate-$(PACKAGE_VERSION).tar gpscorrelate-$(PACKAGE_VERSION)
	-rm gpscorrelate-$(PACKAGE_VERSION).tar.gz
	gzip -9 gpscorrelate-$(PACKAGE_VERSION).tar
	rm -r gpscorrelate-$(PACKAGE_VERSION)
