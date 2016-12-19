# Top-level Makefile for Debian package
#
# If you want a different kind of build, use:
#	cd src; make -f makefile.hpx
# or whatever.
#
#	Martin Guy, 8 April 2006

INSTALLROOT=/usr

BINDIR=$(DESTDIR)$(INSTALLROOT)/bin
HELPDIR=$(DESTDIR)$(INSTALLROOT)/share/xvi
MANDIR=$(DESTDIR)$(INSTALLROOT)/share/man/man1
DOCDIR=$(DESTDIR)$(INSTALLROOT)/share/doc/xvi

INSTALL?=install
MAKE?=make

OPTFLAG = -O2

# Default target for "make"
all:
	@# makefile.pos sets CFLAGS itself; we apply the DEB_BUILD_OPTIONS by
	@# overriding its OPTFLAG variable. Subvert it to set HELPFILE too.
	$(MAKE) -C src -f makefile.pos \
		OPTFLAG="$(OPTFLAG) -DHELPFILE=\\\"$(HELPDIR)/xvi.help\\\"" \
		xvi

check:
	@make -C test check

install: all
	$(INSTALL) -d $(BINDIR) $(HELPDIR) $(MANDIR) $(DOCDIR)
	$(INSTALL) -m 755 -s src/xvi $(BINDIR)
	$(INSTALL) -m 644 src/xvi.help $(HELPDIR)
	$(INSTALL) -m 644 doc/xvi.1 $(MANDIR)

clean:
	$(MAKE) -C src -f makefile.pos clean
	$(MAKE) -C doc clean
