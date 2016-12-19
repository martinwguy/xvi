check:
	@./runtests

nox:
	startx -e "./runtests>errs" -- :1 -quiet -config xorg.conf
	grep -q FAILED errs && exit 1
	rm errs
