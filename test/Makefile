# Makefile for xvi's test suite

check:
	@# Use running X display if any, otherwise try it headless.
	@if [ "$DISPLAY" ]; then ./runtests; else \
	    if [ `whoami` = root ]; then make nox; else \
		echo 'You have to run "make check" under X or be root'; \
	    fi \
	fi

nox:
	@# Run the tests using a dummy X server.
	@test `whoami` = root || { echo "You have to be root"; exit 1; }
	@XAUTHORITY= startx -e "./runtests>errs" -- :1 -config $$PWD/xorg.conf 2>/dev/null
	@cat errs
	@-if grep -q FAILED errs; then exit 1; else rm errs; exit 0; fi

clean:
	rm -f \#unnamed.* *.dump errs
