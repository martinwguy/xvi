/* Copyright (c) 1986 by University of Toronto */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    regexp.h
* module function:
    Definitions for regular expression routines.
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
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 */

#define NSUBEXP 10
typedef struct regexp {
    char    *startp[NSUBEXP];
    char    *endp[NSUBEXP];
    char    regstart;		/* Internal use only. */
    char    reganch;		/* Internal use only. */
    char    *regmust;		/* Internal use only. */
    int     regmlen;		/* Internal use only. */
    char    program[1];		/* Unwarranted chumminess with compiler. */
} regexp;

#ifndef P
#   include "xvi.h"
#endif

/* regerror.c */
extern	void	regerror P((char *s));

/* regexp.c */
extern	regexp	*regcomp P((char *exp));
extern	int	regexec P((regexp *prog, char *string, int at_bol));

/* regsub.c */
extern void	regsub P((regexp *prog, char *source, char *dest));
