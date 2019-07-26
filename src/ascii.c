/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    ascii.c
* module function:
    Visual representations of control & other characters.

    This file is specific to the ASCII character set;
    versions for other character sets could be implemented if
    required.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Modified by Martin Guy
    Cleaned up by C.J.Wagenius

***/

#include "xvi.h"

/*
 * Output visual representation for a character. Return value is the
 * width, in display columns, of the representation; the actual string
 * is returned in *pp unless pp is NULL.
 *
 * If tabcol is -1, a tab is represented as "^I"; otherwise, it gives
 * the current physical column, & is used to determine the number of
 * spaces in the string returned as *pp.
 *
 * Looks at the parameters tabs, list, cchars and mchars.
 * This may sound costly, but in fact it's a single memory
 * access for each parameter.
 */
unsigned vischar(int c, char **p, int idx)
{
	static char buf[max(MAX_TABSTOP+1, 5)];

	if (p) *p = buf;
	/* do we need to complete a tabstop with spaces? */
	if (c == '\t' && idx > -1 && Pb(P_tabs) && !Pb(P_list)) {
		int ts;		/* tabstop          */
		unsigned nspc;	/* number of spaces */

		ts = min(MAX_TABSTOP, Pn(P_tabstop));
		idx %= ts;
		nspc = ts - idx;
		if (p) {
			memset(buf, ' ', nspc);
			buf[nspc] = '\0';
		}
		return nspc;
	} else if (iscntrl(c) && !Pb(P_cchars)) {
		if (p) {
			buf[0] = '^';
			buf[1] = (c == DEL ? '?' : c + ('A' - 1));
			buf[2] = '\0';
		}
		return 2;
	} else if ((c & 0x80) && !Pb(P_mchars)) {
		/* If Pb(P_mchars) is unset, we display non-ASCII characters
		 * (i.e. top-bit-set characters) as hex escape sequences.
		 **/
		if (p) sprintf(buf, "\\x%02x", c);
		return 4;
	} else {
		if (p) sprintf(buf, "%c", c);
		return 1;
	}
}
