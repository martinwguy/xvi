/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    defmain.c
* module function:
    Default main() function; uses default VirtScr implementation
    (defined in defscr.c).
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

extern	VirtScr	defscr;

int
main(argc, argv)
int	argc;
char	*argv[];
{
    defscr_main(argc, argv);
    return(0);
}

void
startup_error(str)
char	*str;
{
    static int	called = 0;

    if (!called) {
	sys_endv();
	called = 1;
    }
    (void) fputs(str, stderr);
}
