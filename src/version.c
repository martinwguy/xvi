/* Copyright (c) 1992,1993,1994 Chris and John Downey */
#ifndef lint
static char *sccsid = "@(#)version.c	2.30 (Chris & John Downey) 8/12/94";
#endif

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    version.c
* module function:
    Version string definition.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey

***/

#ifndef lint
static char *copyright = "@(#)Copyright (c) 1992 Chris & John Downey";
#endif

#ifdef __DATE__
    char	Version[] = "Xvi 2.47 " __DATE__;
#else
    char	Version[] = "Xvi 2.47 10th November 1994";
#endif
