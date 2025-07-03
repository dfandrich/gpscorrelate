# Makefile for gpscorrelate

PACKAGE_VERSION = 2.3.1git

CC = gcc
CXX = g++
EXEEXT =
PKG_CONFIG=pkg-config
CFLAGS   = -Wall -O2
CXXFLAGS = $(CFLAGS)
LDFLAGS  = -lm
GTK      = 3
CHECK_OPTIONS=

COBJS    = main-command.o unixtime.o gpx-read.o correlate.o exif-gps.o latlong.o
GOBJS    = main-gui.o gui.o unixtime.o gpx-read.o correlate.o exif-gps.o latlong.o

# Both BSD make and GNU make >= 4.0 support != to define the flags immediately
# (which calls pkg-config once instead of on every compile), but until that GNU
# make version is widespread, use this slower but more portable form.
CFLAGSINC = `$(PKG_CONFIG) --cflags libxml-2.0 exiv2`
GTKFLAGS  = `$(PKG_CONFIG) --cflags gtk+-$(GTK).0`
LIBS      = `$(PKG_CONFIG) --libs libxml-2.0 exiv2`
LIBSGUI   = `$(PKG_CONFIG) --libs gtk+-$(GTK).0`

CFLAGSINC += $(GTKFLAGS)

# Put --nonet here to avoid downloading DTDs while building documentation
XSLTFLAGS =

prefix   = /usr/local
bindir   = $(prefix)/bin
datadir  = $(prefix)/share
mandir   = $(datadir)/man
docdir   = $(datadir)/doc/gpscorrelate
applicationsdir = $(datadir)/applications
localedir = $(datadir)/locale

DEFS = -DPACKAGE_VERSION=\"$(PACKAGE_VERSION)\" -DPACKAGE_LOCALE_DIR=\"$(localedir)\" -DPACKAGE_DOC_DIR=\"$(docdir)\"

TARGETS = gpscorrelate-gui$(EXEEXT) gpscorrelate$(EXEEXT)

all:    $(TARGETS) docs

gpscorrelate$(EXEEXT): $(COBJS)
	$(CXX) -o $@ $(COBJS) $(LDFLAGS) $(LIBS)

gpscorrelate-gui$(EXEEXT): $(GOBJS)
	$(CXX) -o $@ $(GOBJS) $(LIBSGUI) $(LDFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(CFLAGSINC) $(DEFS) -c -o $@ $<

.cpp.o:
	$(CXX) $(CXXFLAGS) $(CFLAGSINC) $(DEFS) -c -o $@ $<

# Hack to recompile everything if a header changes
*.o: *.h

check: gpscorrelate$(EXEEXT)
	(cd tests && ./testsuite $(CHECK_OPTIONS))

clean:
	rm -f *.o gpscorrelate$(EXEEXT) gpscorrelate-gui$(EXEEXT) doc/gpscorrelate-manpage.xml tests/log/* io.github.dfandrich.gpscorrelate.metainfo.xml $(TARGETS)

distclean: clean clean-po
	rm -f AUTHORS

install-gpscorrelate:
	install -d $(DESTDIR)$(bindir)
	install -m 0755 gpscorrelate$(EXEEXT) $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(mandir)/man1
	install -m 0644 doc/gpscorrelate.1 $(DESTDIR)$(mandir)/man1
	install -d $(DESTDIR)$(docdir)
	install -p -m 0644 doc/*.html doc/*.png README.md $(DESTDIR)$(docdir)
	install -m 0644 gpscorrelate-gui.svg $(DESTDIR)$(docdir)
	install -d $(DESTDIR)$(docdir)/fr
	install -p -m 0644 doc/fr/*.html doc/fr/*.png $(DESTDIR)$(docdir)/fr

install-desktop-file:
	desktop-file-install --vendor="" --dir="$(DESTDIR)$(applicationsdir)" gpscorrelate.desktop
	install -d $(DESTDIR)$(datadir)/icons/hicolor/scalable/apps
	install -p -m 0644 gpscorrelate-gui.svg $(DESTDIR)$(datadir)/icons/hicolor/scalable/apps/gpscorrelate-gui.svg

install-gpscorrelate-gui: install-desktop-file
	install -d $(DESTDIR)$(bindir)
	install -m 0755 gpscorrelate-gui$(EXEEXT) $(DESTDIR)$(bindir)

install: all $(foreach target,$(TARGETS),install-$(target))

docs: doc/gpscorrelate.1 doc/gpscorrelate.html

# BSD make doesn't work with $< as the prerequisite in the following rules but $? is fine
doc/gpscorrelate-manpage.xml: doc/gpscorrelate-manpage.xml.in
	sed -e 's,@DOCDIR@,$(docdir),g' -e 's,@PACKAGE_VERSION@,$(PACKAGE_VERSION),g' $? > $@

doc/gpscorrelate.1: doc/gpscorrelate-manpage.xml
	xsltproc $(XSLTFLAGS) -o $@ http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $?

doc/gpscorrelate.html: doc/gpscorrelate-manpage.xml
	xsltproc $(XSLTFLAGS) -o $@ http://docbook.sourceforge.net/release/xsl/current/html/docbook.xsl $?

# This is intended to be used only by the maintainer
io.github.dfandrich.gpscorrelate.metainfo.xml: io.github.dfandrich.gpscorrelate.metainfo.xml.in
	intltool-merge -x po $< $@
	echo '' >> $@  # the previous command drops the trailing \n
	# Template substitutions plus hack to remove period to avoid having nearly duplicate translations
	sed -E -e "s,@PACKAGE_VERSION@,$(PACKAGE_VERSION),g" -e "s,@DATE@,`date +%F`,g" -e 's,\.(</summary>),\1,' $@ > $@.tmp
	mv -f $@.tmp $@

build-po:
	(cd po && $(MAKE) VERSION="$(PACKAGE_VERSION)" prefix="$(prefix)" top_srcdir="$(PWD)" update-po)
	(cd po && $(MAKE) VERSION="$(PACKAGE_VERSION)" prefix="$(prefix)" top_srcdir="$(PWD)" all)

install-po: build-po
	(cd po && $(MAKE) VERSION="$(PACKAGE_VERSION)" prefix="$(prefix)" top_srcdir="$(PWD)" install)

clean-po:
	(cd po && $(MAKE) VERSION="$(PACKAGE_VERSION)" prefix="$(prefix)" top_srcdir="$(PWD)" clean)

AUTHORS: AUTHORS.extra
	# Include authors here who aren't in the git commits
	(git log HEAD | sed -n -e '/^Author:/s/^[^:]*: //p' && cat AUTHORS.extra ) | sort -u > $@

# Create a distribution archive
dist: AUTHORS io.github.dfandrich.gpscorrelate.metainfo.xml docs
	mkdir gpscorrelate-$(PACKAGE_VERSION)
	git archive --prefix=gpscorrelate-$(PACKAGE_VERSION)/ HEAD | tar xf -
	install -m 0644 AUTHORS io.github.dfandrich.gpscorrelate.metainfo.xml gpscorrelate-$(PACKAGE_VERSION)
	install -m 0644 doc/gpscorrelate.1 doc/gpscorrelate-manpage.xml doc/gpscorrelate.html gpscorrelate-$(PACKAGE_VERSION)/doc
	-rm gpscorrelate-$(PACKAGE_VERSION)/po/stamp-po
	cd gpscorrelate-$(PACKAGE_VERSION)/po && $(MAKE) VERSION="$(PACKAGE_VERSION)" prefix="$(prefix)" top_srcdir="$(PWD)" gpscorrelate.pot-update clean
	-rm gpscorrelate-$(PACKAGE_VERSION)/po/stamp-po
	tar cf gpscorrelate-$(PACKAGE_VERSION).tar gpscorrelate-$(PACKAGE_VERSION)
	-rm gpscorrelate-$(PACKAGE_VERSION).tar.gz
	gzip -9 gpscorrelate-$(PACKAGE_VERSION).tar
	rm -r gpscorrelate-$(PACKAGE_VERSION)
