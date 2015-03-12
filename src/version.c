/* Copyright (c) 1992,1993,1994 Chris and John Downey */

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
    Last modified by Martin Guy

***/

#ifndef lint
static char *copyright = "@(#)Copyright (c) 1992 Chris & John Downey";
#endif

#ifdef __DATE__
    char	Version[] = "Xvi 2.49 " __DATE__;
#else
    char	Version[] = "Xvi 2.49 4th March 2015";
#endif
