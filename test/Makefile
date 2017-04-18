# Makefile for xvi's test suite

all:
	@make -C ..

check:
	@scripts/runtests

nox:
	@# Run the tests using a dummy X server.
	@test `whoami` = root || { echo "You have to be root"; exit 1; }
	@XAUTHORITY= startx -e "scripts/runtests>errs" -- :1 -config $$PWD/data/xorg.conf 2>/dev/null
	@cat errs
	@-if grep -q FAILED errs; then exit 1; else rm errs; exit 0; fi

clean:
	@# vim creates .swp files
	rm -f \#unnamed.* *.dump *.tmp .sw? core errs typescript
