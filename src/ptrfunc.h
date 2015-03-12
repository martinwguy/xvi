/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    ptrfunc.h
* module function:
    Functions on Posn's - defined here for speed, since they
    typically get called "on a per-character basis".
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

/*
 * gchar(lp) - get the character at position "lp"
 */
#define	gchar(lp)	((lp)->p_line->l_text[(lp)->p_index])
