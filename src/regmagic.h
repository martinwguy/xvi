/* Copyright (c) 1986 by University of Toronto */

/***

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
    Last modified by Martin Guy

***/

/*
 *	Copyright (c) 1986 by University of Toronto.
 *	Written by Henry Spencer.  Not derived from licensed software.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to redistribute it freely,
 *	subject to the following restrictions:
 *
 *	1. The author is not responsible for the consequences of use of
 *		this software, no matter how awful, even if they arise
 *		from defects in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *		by explicit claim or by omission.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *		be misrepresented as being the original software.
 */

/*
 * The first byte of the regexp internal "program" is actually this magic
 * number; the start node begins in the second byte.
 */
#define	MAGIC	0234
