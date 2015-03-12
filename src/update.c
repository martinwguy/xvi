/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    update.c
* module function:
    Update what is on the screen, according to what is in the
    internal and external screen buffers (presumably the internal
    one has been changed).
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

static	void	xvUpdateLine P((Xviwin *, VirtScr *, int));
static	void	xvWriteMultiString
		    P((VirtScr *, char *, int, unsigned char *, int, int));

/*
 * Transfer the contents of int_lines to the screen,
 * starting at "start_row", for "nlines" lines,
 * using ext_lines to avoid unnecessary output.
 */
void
xvUpdateScr(win, vs, start_row, nlines)
Xviwin			*win;
register VirtScr	*vs;
int			start_row;
int			nlines;
{
    register int	row;		/* current row */
    int         	end_row;	/* row after last one to be updated */

    if (!(echo & e_CHARUPDATE)) {
	return;
    }

    end_row = start_row + nlines;

    VSset_colour(vs, VSCcolour);

    for (row = start_row; row < end_row; row++) {
	register unsigned	nflags;
	register unsigned	rflags;		/* flags for current lines */

	nflags = vs->pv_int_lines[row].s_flags;
	rflags = vs->pv_ext_lines[row].s_flags;

	/*
	 * Only update the line if necessary.
	 */
	if ((nflags & S_DIRTY) || (rflags & S_DIRTY)) {
	    xvUpdateLine(win, vs, row);

	    /*
	     * The real screen line is a message or command line if the
	     * newly-updated one was. Otherwise, it isn't.
	     *
	     * Both the new and real screens may now be considered "clean".
	     */
	    vs->pv_ext_lines[row].s_flags = (
		/*
		 * Turn these flags off first ...
		 */
		(rflags & ~(S_STATUS | S_DIRTY))
		/*
		 * ... then set whatever S_STATUS flags are
		 * set in int_lines.
		 */
		| (nflags & S_STATUS)
	    );
	    vs->pv_int_lines[row].s_flags &= ~S_DIRTY;
	}
    }
    VSflush(vs);
}

/*
 * Update the given line to the screen. This routine is only called
 * if either the internal image, or the screen itself, has changed
 * (i.e. is marked "dirty").
 */
static void
xvUpdateLine(win, vs, row)
Xviwin		*win;
VirtScr		*vs;
int		row;
{
    Sline		*new;		/* pointers to current lines */
    Sline		*real;
    register char	*ntextp;	/* pointers to line text */
    register char	*rtextp;
    unsigned char	*ncolours;	/* pointers to character colours */
    unsigned char	*rcolours;
    int			n_used;
    int			r_used;
    int			columns;
    bool_t		ncoloured;	/* TRUE if ntextp has colour */
    int			i;
    register int	col;		/* current column */
    register int	adv_col;	/* column after last to be updated */
    bool_t		do_advise;	/* TRUE if VSadvise() is advisory */
    unsigned		last_colour;	/* colour of last char written out */

    columns = VScols(vs);

    new = vs->pv_int_lines + row;
    real = vs->pv_ext_lines + row;

    ntextp = new->s_line;
    rtextp = real->s_line;
    ncolours = new->s_colour;
    rcolours = real->s_colour;

    n_used = new->s_used;
    r_used = real->s_used;

    ncoloured = FALSE;
    for (i = 0; i < n_used; i++) {
	if (ntextp[i] != VSCcolour) {
	    ncoloured = TRUE;
	    break;
	}
    }

    /*
     * Look at each character in the line, comparing the new version
     * with the one on the screen. If they differ, put the char out.
     *
     * adv_col is the column following the last one to be updated;
     * we use it for VSadvise().
     *
     * Note that this loop needs to be as tight as possible,
     * since it is the core of screen updating.
     */
    do_advise = TRUE;
    last_colour = VSCcolour;
    for (col = adv_col = 0; col < n_used && col < r_used; col++) {
	register int	nc;

	nc = ntextp[col];
	if (nc != rtextp[col] || ncolours[col] != rcolours[col]) {
	    /*
	     * They are different. Get to the right
	     * place before putting out the char.
	     */
	    if (col > adv_col && do_advise) {
		VSadvise(vs, row, adv_col, col - adv_col,
			 ntextp + adv_col);
	    }

	    VSset_colour(vs, ncolours[col]);
	    VSputc(vs, row, col, nc);

	    adv_col = col + 1;
	    do_advise = TRUE;
	    last_colour = ncolours[col];
	} else if (ncolours[col] != last_colour) {
	    /*
	     * If the colour has changed since adv_col, we cannot
	     * perform a VSadvise(), so reset do_advise here to
	     * prevent it from happening.
	     */
	    do_advise = FALSE;
	}
    }

    if (n_used > r_used) {
	/*
	 * We have got to the end of the previous
	 * screen line; if there is anything left,
	 * we should just display it.
	 */

	/*
	 * Skip over initial (background-coloured) whitespace.
	 */
	while (ntextp[col] == ' ' && ncolours[col] == VSCcolour) {
	    col++;
	}
	if (col < columns) {
	    if (do_advise && col > adv_col) {
		VSadvise(vs, row, adv_col, col - adv_col, ntextp + adv_col);
	    }
	    xvWriteMultiString(vs, &ntextp[col], n_used - col,
				    &ncolours[col], row, col);
	}
    } else if (r_used > n_used) {
	/*
	 * We have got to the end of the new screen
	 * line, but the old one still has stuff on
	 * it. We must therefore clear it.
	 */
	VSclear_line(vs, row, col);
    }

    if (ncoloured && n_used < columns) {
	VSset_colour(vs, VSCcolour);
	VSclear_line(vs, row, n_used);
    }

    (void) memcpy(real->s_line, new->s_line, n_used);
    (void) memcpy(real->s_colour, new->s_colour, n_used);
    real->s_line[n_used] = '\0';
    real->s_used = n_used;
}

/*
 * Write a string containing multiple coloured sections at the given
 * location. The colours are given by the byte array "colours".
 */
static void
xvWriteMultiString(vs, str, length, colours, row, col)
VirtScr		*vs;
register char	*str;
int		length;
unsigned char	*colours;
int		row;
register int	col;
{
    register int	i;
    unsigned		last_colour;

    VSset_colour(vs, colours[0]);
    last_colour = colours[0];
    for (i = 0; i < length; i++) {
	if (colours[i] != last_colour) {
	    VSset_colour(vs, colours[i]);
	    last_colour = colours[i];
	}
	VSputc(vs, row, col++, str[i]);
    }
    VSset_colour(vs, VSCcolour);
}

/*
 * Set the S_DIRTY bit for a given line in both ext_lines &
 * int_lines if the stored representations are in fact different:
 * otherwise clear it. The line is an index into the VirtScr.
 */
void
xvMarkDirty(vs, row)
VirtScr	*vs;
int	row;
{
    Sline	*rp;
    Sline	*np;
    int		used;

    rp = vs->pv_ext_lines + row;
    np = vs->pv_int_lines + row;
    if (

	(rp->s_flags & ~S_DIRTY) != (np->s_flags & ~S_DIRTY)
	||
	(used = rp->s_used) != np->s_used
	||
	memcmp(rp->s_line, np->s_line, used) != 0
	||
	memcmp(rp->s_colour, np->s_colour, used) != 0
    ) {
	/*
	 * The lines are different.
	 */
	np->s_flags |= S_DIRTY;
	rp->s_flags |= S_DIRTY;
    } else {
	rp->s_flags = (np->s_flags &= ~S_DIRTY);
    }
}

/*
 * Clear the given line, marking it as dirty.
 * The line is a VirtScr index.
 */
void
xvClearLine(vs, line)
VirtScr		*vs;
unsigned	line;
{
    vs->pv_ext_lines[line].s_used = 0;
    vs->pv_ext_lines[line].s_line[0] = '\0';
    xvMarkDirty(vs, (int) line);
}

void
xvClear(vs)
VirtScr	*vs;
{
    register unsigned	row;
    register unsigned	nrows;

    nrows = VSrows(vs);

    VSset_colour(vs, VSCcolour);
    VSclear_all(vs);

    /*
     * Clear the real screen lines, and mark them as modified.
     */
    for (row = 0; row < nrows; row++) {
	xvClearLine(vs, row);
    }
}
