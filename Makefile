# Top-level makefile for Debian package
#
#	Martin Guy, 8 April 2006

BINDIR=$(DESTDIR)/usr/bin
HELPDIR=$(DESTDIR)/usr/share/xvi
MANDIR=$(DESTDIR)/usr/share/man/man1
DOCDIR=$(DESTDIR)/usr/share/doc/xvi

PROG=src/xvi

# Default target for "make"
$(PROG):
	# The makefile.lnx sets CFLAGS itself; we apply the DEB_BUILD_OPTIONS
	# by overriding its OPTFLAG variable and subvert it to set HELPFILE too.
	make -C src -f makefile.lnx \
		OPTFLAG="$(OPTFLAG) -DHELPFILE=\\\"$(HELPDIR)/xvi.help\\\"" \
		xvi

install: $(PROG)
	install -d $(BINDIR) $(HELPDIR) $(MANDIR) $(DOCDIR)
	install -m 755 src/xvi $(BINDIR)
	install -m 644 src/xvi.help $(HELPDIR)
	install -m 644 doc/xvi.1 $(MANDIR)
	# Need to remove backspacing from nroff output
	col -b < doc/summary.lst > $(DOCDIR)/summary.txt
	chmod 644 $(DOCDIR)/summary.txt

clean:
	rm -f src/*.o src/xvi
