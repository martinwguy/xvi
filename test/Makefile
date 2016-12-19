check:
	@./runtests

nox:
	@test `whoami` = root || { echo "You have to be root"; exit 1; }
	@XAUTHORITY= startx -e "./runtests>errs" -- :1 -config $$PWD/xorg.conf 2>/dev/null
	@-if grep FAILED errs; then exit 1; else rm -f errs; exit 0; fi
