/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    mark.c
* module function:
    Routines to maintain and manipulate marks.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

#ifdef	MEGAMAX
overlay "mark"
#endif

/*
 * A new buffer - initialise it so there are no marks.
 */
void
init_marks(buffer)
Buffer	*buffer;
{
    Mark	*mlist = buffer->b_mlist;
    int	i;

    /* Set all m_line to NULL */
    memset(mlist, sizeof(*mlist) * NMARKS, 0);
    buffer->b_pcmark.m_line = NULL;
}

/*
 * setmark(c) - set mark 'c' at current cursor position in given buffer.
 *
 * Returns TRUE on success, FALSE if no room for mark or bad name given.
 */
bool_t
setmark(c, buffer, pos)
int	c;
Buffer	*buffer;
Posn	*pos;
{
    if (c == '\'' || c == '`') {
	buffer->b_pcmark.m_line = pos->p_line;
	return(TRUE);
    }
    if (is_lower(c)) {
	buffer->b_mlist[c - 'a'].m_line = pos->p_line;
	return(TRUE);
    }
    return(FALSE);
}

/*
 * setpcmark() - set the previous context mark to the current position
 */
void
setpcmark(window)
Xviwin	*window;
{
    window->w_buffer->b_pcmark.m_line = window->w_cursor->p_line;
}

/*
 * getmark(c) - find mark for char 'c' in given buffer
 *
 * Return pointer to Line or NULL if no such mark.
 */
Line *
getmark(c, buffer)
int	c;
Buffer	*buffer;
{
    if (c == '\'' || c == '`') {
	return(buffer->b_pcmark.m_line);
    }
    if (is_lower(c)) {
	return(buffer->b_mlist[c - 'a'].m_line);
    }
    return(NULL);
}

/*
 * clrmark(line) - clear any marks for 'line'
 *
 * Used any time a line is deleted.
 */
void
clrmark(line, buffer)
Line	*line;
Buffer	*buffer;
{
    Mark	*mlist = buffer->b_mlist;
    int	i;

    for (i = 0; i < NMARKS; i++) {
	if (mlist[i].m_line == line) {
	    mlist[i].m_line = NULL;
	}
    }
    if (buffer->b_pcmark.m_line == line) {
	buffer->b_pcmark.m_line = NULL;
    }
}
