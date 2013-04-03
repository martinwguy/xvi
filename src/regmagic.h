/***

* @(#)regmagic.h	2.3 4/27/93

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    regmagic.h
* module function:
    Definition of magic number for regular expression routines.

* history:
    Regular expression routines by Henry Spencer.
    Modfied for use with STEVIE (ST Editor for VI Enthusiasts,
     Version 3.10) by Tony Andrews.
    Adapted for use with Xvi by Chris & John Downey.
    Original copyright notice is in regexp.c.
    Please note that this is a modified version.
***/

/*
 * The first byte of the regexp internal "program" is actually this magic
 * number; the start node begins in the second byte.
 */
#define	MAGIC	0234
