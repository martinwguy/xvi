# Top-level makefile for Debian package
#
#	Martin Guy, 8 April 2006

INSTALLROOT=/usr

BINDIR=$(DESTDIR)$(INSTALLROOT)/bin
HELPDIR=$(DESTDIR)$(INSTALLROOT)/share/xvi
MANDIR=$(DESTDIR)$(INSTALLROOT)/share/man/man1
DOCDIR=$(DESTDIR)$(INSTALLROOT)/share/doc/xvi

# Default target for "make"
all:
	@# makefile.lnx sets CFLAGS itself; we apply the DEB_BUILD_OPTIONS by
	@# overriding its OPTFLAG variable. Subvert it to set HELPFILE too.
	make -C src -f makefile.lnx \
		OPTFLAG="$(OPTFLAG) -DHELPFILE=\\\"$(HELPDIR)/xvi.help\\\"" \
		xvi

install: all
	install -d $(BINDIR) $(HELPDIR) $(MANDIR) $(DOCDIR)
	install -m 755 -s src/xvi $(BINDIR)
	install -m 644 src/xvi.help $(HELPDIR)
	install -m 644 doc/xvi.1 $(MANDIR)
	# Need to remove backspacing from nroff output
	nroff -ms < doc/summary.ms | col -b > $(DOCDIR)/summary.txt
	chmod 644 $(DOCDIR)/summary.txt

clean:
	rm -f src/*.o src/xvi
