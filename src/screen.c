/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    screen.c
* module function:
    Screen handling functions.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

static	int	line_to_new P((Line *, int, long));
static	void	file_to_new P((void));
static	void	do_sline P((void));

/*
 * Transfer the specified window line into the "new" screen array, at
 * the given row. Returns the number of screen lines taken up by the
 * logical buffer line lp, or 0 if the line would not fit; this happens
 * with longlines at the end of the screen. In this case, the lines
 * which could not be displayed will have been marked with an '@'.
 */
static int
line_to_new(lp, start_row, line)
Line		*lp;
int		start_row;
long		line;
{
    Xviwin		*win = curwin;
    register unsigned	c;		/* next character from file */
    register Sline	*curr_line;	/* output line - used for efficiency */
    register char	*ltext;		/* pointer to text of line */
    register int	curr_index;	/* current index in line */
    bool_t		eoln;		/* true when line is done */
    char		extra[MAX_TABSTOP];
					/* Stack for extra characters. */
    int			nextra = 0;	/* index into stack */
    int			srow, scol;	/* current screen row and column */
    int			vcol;		/* virtual column */
    int			norm_colour_pos;/* pos where we switch off highlight */
    unsigned		colour;		/* current plotting colour */

    ltext = lp->l_text;
    srow = start_row;
    scol = vcol = 0;
    curr_line = win->w_vs->pv_int_lines + srow;
    curr_index = 0;
    norm_colour_pos = 0;
    colour = VSCcolour;
    eoln = FALSE;

    if (Pb(P_number)) {
	static Flexbuf	ftmp;

	flexclear(&ftmp);
	(void) lformat(&ftmp, NUM_FMT, line);
	(void) strcpy(curr_line->s_line, flexgetstr(&ftmp));
	for (scol = 0; scol < NUM_SIZE; scol++) {
	    curr_line->s_colour[scol] = VSCcolour;
	}
	/* assert: scol == NUM_SIZE */
    }

    while (!eoln) {
	/*
	 * Get the next character to put on the screen.
	 */

	/*
	 * "extra" is a stack containing any extra characters
	 * we have to put on the screen - this is for chars
	 * which have a multi-character representation, and
	 * for the $ at end-of-line in list mode.
	 */

	if (nextra > 0) {
	    c = extra[--nextra];
	} else {
	    unsigned	n;

	    if (curr_index == norm_colour_pos) {
		colour = VSCcolour;
	    }

	    c = (unsigned char) (ltext[curr_index++]);

	    /*
	     * Deal with situations where it is not
	     * appropriate just to copy characters
	     * straight onto the screen.
	     */
	    if (c == '\0') {

		if (Pb(P_list)) {
		    /*
		     * Have to show a '$' sign in list mode.
		     */
		    extra[nextra++] = '\0';
		    c = '$';
		}

	    } else {
		char	*p;

		n = vischar((int) c, &p, vcol);
		/*
		 * This is a bit paranoid assuming
		 * that Pn(P_tabstop) can never be
		 * greater than sizeof (extra), but
		 * so what.
		 */
		if (nextra + n > sizeof extra) {
		    n = sizeof(extra) - nextra;
		}
		/*
		 * Stack the extra characters so that
		 * they appear in the right order.
		 */
		while (n > 1) {
		    extra[nextra++] = p[--n];
		}
		c = p[0];
	    }
	}

	if (c == '\0') {
	    /*
	     * End of line. Terminate it and finish.
	     */
	    eoln = TRUE;
	    curr_line->s_flags = S_TEXT;
	    curr_line->s_used = scol;
	    curr_line->s_line[scol] = '\0';
	    xvMarkDirty(srow);
	    break;
	} else {
	    /*
	     * Sline folding.
	     */
	    if (scol >= win->w_ncols) {
		curr_line->s_flags = S_TEXT;
		curr_line->s_used = scol;
		curr_line->s_line[scol] = '\0';
		xvMarkDirty(srow);
		srow += 1;
		scol = 0;
		curr_line = win->w_vs->pv_int_lines + srow;
	    }

	    if (srow >= win->w_cmdline) {
		for (srow = start_row; srow < win->w_cmdline; srow++) {
		    curr_line = win->w_vs->pv_int_lines + srow;

		    curr_line->s_flags = S_MARKER;
		    curr_line->s_used = 1;
		    curr_line->s_line[0] = '@';
		    curr_line->s_line[1] = '\0';
		    curr_line->s_colour[0] = VSCcolour;
		    xvMarkDirty(srow);
		}
		return(0);
	    }

	    /*
	     * Store the character in int_lines.
	     */
	    curr_line->s_line[scol] = c;
	    curr_line->s_colour[scol] = colour;
	    scol++;
	    vcol++;
	}
    }

    return((srow - start_row) + 1);
}

/*
 * file_to_new()
 *
 * Based on the current value of topline, transfer a screenful
 * of stuff from file to int_lines, and update botline.
 */
static void
file_to_new()
{
    Xviwin		*win = curwin;
    register int	row;
    register Line	*line;
    long		lnum;

    if (win->w_nrows < Pn(P_minrows))
	return;

    row = win->w_winpos;
    line = win->w_topline;
    lnum = lineno(line);

    while (row < win->w_cmdline && line != curbuf->b_lastline) {
	int nlines;

	nlines = line_to_new(line, row, lnum);
	if (nlines == 0) {
	    /*
	     * Make it look like we have updated
	     * all the screen lines, since they
	     * have '@' signs on them.
	     */
	    row = win->w_cmdline;
	    break;
	} else {
	    row += nlines;
	    line = line->l_next;
	    lnum++;
	}
    }

    win->w_botline = line;

    /*
     * If there are any lines remaining, fill them in
     * with '~' characters.
     */
    for ( ; row < win->w_cmdline; row++) {
	register Sline	*curr_line;

	curr_line = win->w_vs->pv_int_lines + row;

	curr_line->s_flags = S_MARKER;
	curr_line->s_used = 1;
	curr_line->s_line[0] = '~';
	curr_line->s_line[1] = '\0';
	curr_line->s_colour[0] = VSCcolour;
	xvMarkDirty(row);
    }
}

/*
 * Update the status line of the given window, and cause the status
 * line to be written out. Note that we call xvUpdateScr() to cause
 * the output to be generated; since there will be no other changes,
 * only the status line will be changed on the screen.
 */
void
update_sline()
{
    do_sline();
    xvUpdateScr((int) curwin->w_cmdline, 1);
}

/*
 * Update the status line of the current window,
 * from the one in curwin->w_statusline.
 */
static void
do_sline()
{
    Xviwin		*win = curwin;
    VirtScr		*vs = curwin->w_vs;
    register char	*from;
    register char	*to;
    register char	*end;
    int			colindex;
    unsigned		colour;
    Sline		*slp;

    if ((win->w_nrows == 0) ||
	(VSrows(vs)  == 0) ||
	(VScols(vs) == 0)) {
	return;
    }

    from = sline_text(win);
    slp = vs->pv_int_lines + win->w_cmdline;
    to = slp->s_line;
    end = to + win->w_ncols - SPARE_COLS;

    while (*from != '\0' && to < end) {
	*to++ = *from++;
    }

    /*
     * Fill with spaces, and null-terminate.
     */
    while (to < end) {
	*to++ = ' ';
    }
    *to = '\0';

    if(Pb(P_showmode)) {
	char *s;
	switch (State) {
	case INSERT:
	    s = "INSERT MODE";
	    break;
	case REPLACE:
	    s = "REPLACE MODE";
	    break;
	default:
	    s = NULL;
	}
	if (s && win->w_ncols > 20) {
	    strncpy(slp->s_line + win->w_ncols - 20, s, strlen(s));
        }
    }

    /*
     * Set the colour of the status line.
     */
    colour = is_readonly(curbuf) ? VSCroscolour : VSCstatuscolour;
    for (colindex = win->w_ncols - SPARE_COLS - 1; colindex >= 0;
							--colindex) {
	slp->s_colour[colindex] = colour;
    }

    slp->s_used = win->w_ncols - SPARE_COLS;
    slp->s_flags = S_MESSAGE;
    if (is_readonly(curbuf)) {
	slp->s_flags |= S_READONLY;
    }
    xvMarkDirty((int) win->w_cmdline);
}

/*
 * update_cline()
 *
 * Update the status line from the command line buffer.
 */
void
update_cline()
{
    Xviwin	*win = curwin;
    Sline	*clp;
    size_t	cont, width, maxwidth;
    int		colindex, start, col;
    char *cbuf = flexgetstr(&curwin->w_statusline);
    int		pos = get_pos();	/* Position of cursor within line */

    clp = win->w_vs->pv_int_lines + win->w_cmdline;
    cont = 0;
    maxwidth = win->w_ncols - SPARE_COLS;
    start = (pos > maxwidth) ? (pos / maxwidth) * maxwidth : 0;
    width = strlen(cbuf+start);
    col = (pos > maxwidth) ? (pos - start) + 1 : pos;

    if (width > maxwidth) {
	width = maxwidth;
    }

    if (start) {
	clp->s_line[cont++] = '<';
    }

    (void) strncpy(clp->s_line+cont, cbuf+start, width);
    clp->s_used = width+cont;
    clp->s_line[width+cont] = '\0';
    clp->s_flags = (S_COMMAND | S_DIRTY);
    if (is_readonly(curbuf)) {
	clp->s_flags |= S_READONLY;
    }

    /*
     * Set the colour of the command line. The first character is the same
     * colour as the status line would be, the rest is the normal colour of
     * the screen text (this is better for screen-updating purposes).
     */
    clp->s_colour[0] = is_readonly(curbuf) ?
					VSCroscolour : VSCstatuscolour;
    for (colindex = clp->s_used - 1; colindex >= 1; --colindex) {
	clp->s_colour[colindex] = VSCcolour;
    }

    /*
     * We don't bother calling xvMarkDirty() here: it isn't worth
     * it because the line's contents have almost certainly changed.
     */
    xvUpdateScr((int) win->w_cmdline, 1);
    VSgoto(win->w_vs, (int) win->w_cmdline, col);
}

/*
 * updateline() - update the line the cursor is on
 *
 * Updateline() is called after changes that only affect the line that
 * the cursor is on. This improves performance tremendously for normal
 * insert mode operation. The only thing we have to watch for is when
 * the cursor line grows or shrinks around a row boundary. This means
 * we have to repaint other parts of the screen appropriately.
 * If the flag argument is TRUE, all lines below the current line are
 * also updated.
 */
void
updateline(flag)
bool_t	flag;
{
    Xviwin	*savecurwin;
    Line	*currline;
    int		nlines;
    int		curs_row;

    currline = curwin->w_cursor->p_line;

    /* Loop curwin through all the windows */
    savecurwin = curwin;
    do {
	Xviwin *w = curwin;	/* local temp for faster/smaller code */

	if (w->w_buffer == savecurwin->w_buffer &&
		!earlier(currline, w->w_topline) &&
		!later(currline, w->w_botline)) {
	    /*
	     * Find out which screen line the cursor line starts on.
	     * This is not necessarily the same as window->w_row,
	     * because longlines are different.
	     */
	    curs_row = (int) cntplines(w->w_topline, currline);

	    nlines = line_to_new(currline,
				 (int) (curs_row + w->w_winpos),
				 (long) lineno(currline));
	    if (flag) {
		nlines = w->w_nrows - curs_row - 1;
		file_to_new();
	    }

	    xvUpdateScr((int) (curs_row + w->w_winpos), nlines);
	}
	set_curwin(xvNextDisplayedWindow(w));
    } while (curwin != savecurwin);
}

/*
 * Completely update the representation of the given window.
 * If the flag argument is TRUE, we also update the status line
 * and clear the window beforehand.
 */
void
redraw_window(flag)
bool_t	flag;
{
    if (curwin == NULL || curwin->w_nrows == 0) {
	return;
    }
    if (flag) {
	xvClear();
	update_sline();
    }
    if (curwin->w_nrows > 1) {
	file_to_new();
	xvUpdateScr( (int) curwin->w_winpos, (int) curwin->w_nrows);
    }
}

/*
 * Update all windows.
 */
void
redraw_all(clrflag)
bool_t	clrflag;
{
    Xviwin	*savecurwin;
    VirtScr	*vs = curwin->w_vs;

    if ((VSrows(vs) < Pn(P_minrows)) || (VScols(vs) == 0)) {
	return;
    }

    if (clrflag) {
	xvClear();
    }

    savecurwin = curwin;
    do {
	if (curwin->w_nrows > 0) {
	    if (curwin->w_nrows >= Pn(P_minrows))
		file_to_new();
	    do_sline();
	}
        set_curwin(xvNextDisplayedWindow(curwin));
    } while (curwin != savecurwin);

    xvUpdateScr(0, (int) VSrows(vs));
}

/*
 * The rest of the routines in this file perform screen manipulations.
 * The given operation is performed physically on the screen. The
 * corresponding change is also made to the internal screen image. In
 * this way, the editor anticipates the effect of editing changes on
 * the appearance of the screen. That way, when we call screenupdate a
 * complete redraw isn't usually necessary. Another advantage is that
 * we can keep adding code to anticipate screen changes, and in the
 * meantime, everything still works.
 */

/*
 * s_ins(row, nlines) - insert 'nlines' lines at 'row'
 */
void
s_ins(row, nlines)
register int	row;
int		nlines;
{
    Xviwin		*win = curwin;
    register int	from, to;
    int			count;
    int			bottomline;
    enum
    {
	cant_do_it,
	do_cmdline,
	do_as_requested
    }			strategy;
    VirtScr		*vs;

    if (!(echo & e_SCROLL))
	return;

    /*
     * There's no point in scrolling more lines than there are
     * (below row) in the window, or in scrolling 0 lines.
     */
    if (nlines == 0 || nlines + row >= win->w_nrows - 1)
	return;

    /*
     * The row specified is relative to the top of the window;
     * add the appropriate offset to make it into a screen row.
     */
    row += win->w_winpos;

    bottomline = win->w_cmdline - 1;

    /*
     * Note that we avoid the use of 1-line scroll regions; these
     * only ever occur at the bottom of a window, and it is better
     * just to leave the line to be updated in the best way by
     * update{line,screen}.
     */
    if (nlines == 1 && row == bottomline) {
	return;
    }

    vs = win->w_vs;

    strategy = VScan_scroll(vs, row, bottomline, -nlines) ?
		    do_as_requested :
		    (VScan_scroll(vs, row, bottomline + 1, -nlines) ?
			do_cmdline : cant_do_it);

    switch (strategy) {
    case do_cmdline:
	/*
	 * Can't scroll what we were asked to - try scrolling
	 * the whole window including the status line.
	 */
	bottomline++;
	VSclear_line(vs, bottomline, 0);
	xvClearLine(bottomline);
	/*FALLTHROUGH*/

    case do_as_requested:
	if (!VSscroll(vs, row, bottomline, -nlines)) {
	    /*
	     * Failed.
	     */
	    return;
	}
	/*
	 * Update the stored screen image so it matches what has
	 * happened on the screen.
	 */

	/*
	 * Move section of text down to the bottom.
	 *
	 * We do this by rearranging the pointers within the Slines,
	 * rather than copying the characters.
	 */
	for (to = win->w_cmdline - 1, from = to - nlines; from >= row;
						    --from, --to) {
	    register Sline	*lpfrom;
	    register Sline	*lpto;
	    register char	*temp;

	    lpfrom = &vs->pv_ext_lines[from];
	    lpto = &vs->pv_ext_lines[to];

	    temp = lpto->s_line;
	    lpto->s_line = lpfrom->s_line;
	    lpfrom->s_line = temp;
	    lpto->s_used = lpfrom->s_used;
	}

	/*
	 * Clear the newly inserted lines.
	 */
	for (count = row; count < row + nlines; count++) {
	    xvClearLine((unsigned) count);
	}

    case cant_do_it:
	break;
    }
}

/*
 * s_del(row, nlines) - delete 'nlines' lines starting at 'row'.
 */
void
s_del(row, nlines)
int			row;
int			nlines;
{
    register int	from, to;
    int			count;
    int			bottomline;
    enum
    {
	cant_do_it,
	do_cmdline,
	do_as_requested
    }			strategy;
    VirtScr		*vs;

    if (!(echo & e_SCROLL))
	return;

    /*
     * There's no point in scrolling more lines than there are
     * (below row) in the window, or in scrolling 0 lines.
     */
    if (nlines == 0 || nlines + row >= curwin->w_nrows - 1)
	return;

    /*
     * The row specified is relative to the top of the window;
     * add the appropriate offset to make it into a screen row.
     */
    row += curwin->w_winpos;

    bottomline = curwin->w_cmdline - 1;

    /*
     * We avoid the use of 1-line scroll regions, since they don't
     * work with many terminals, especially if we are using
     * (termcap) DO to scroll the region.
     */
    if (nlines == 1 && row == bottomline) {
	return;
    }

    vs = curwin->w_vs;

    strategy = VScan_scroll(vs, row, bottomline, nlines) ?
		    do_as_requested :
		    (VScan_scroll(vs, row, bottomline + 1, nlines) ?
			do_cmdline : cant_do_it);

    switch (strategy) {
    case do_cmdline:
	/*
	 * Can't scroll what we were asked to - try scrolling
	 * the whole window including the status line.
	 */
	bottomline++;
	VSclear_line(vs, bottomline, 0);
	xvClearLine(bottomline);
	/*FALLTHROUGH*/

    case do_as_requested:
	if (!VSscroll(vs, row, bottomline, nlines)) {
	    /*
	     * Failed.
	     */
	    return;
	}
	/*
	 * Update the stored screen image so it matches what has
	 * happened on the screen.
	 */

	/*
	 * Move section of text up from the bottom.
	 *
	 * We do this by rearranging the pointers within the Slines,
	 * rather than copying the characters.
	 */
	for (to = row, from = to + nlines; from < curwin->w_cmdline;
						    from++, to++) {
	    register Sline	*lpfrom;
	    register Sline	*lpto;
	    register char	*temp;

	    lpfrom = &vs->pv_ext_lines[from];
	    lpto = &vs->pv_ext_lines[to];

	    temp = lpto->s_line;
	    lpto->s_line = lpfrom->s_line;
	    lpfrom->s_line = temp;
	    lpto->s_used = lpfrom->s_used;
	}

	/*
	 * Clear the deleted lines.
	 */
	bottomline = curwin->w_cmdline;
	for (count = bottomline - nlines; count < bottomline; count++) {
	    xvClearLine((unsigned) count);
	}

    case cant_do_it:
	break;
    }
}

/*
 * Insert a character at the cursor position, updating the screen as
 * necessary. Note that this routine doesn't have to do anything, as
 * the screen will eventually be correctly updated anyway; it's just
 * here for speed of screen updating.
 */
void
s_inschar(newchar)
int			newchar;
{
    Sline		*rp;
    Posn		*pp;
    VirtScr		*vs;		/* the VirtScr for this window */
    char		*newstr;	/* printable string for newchar */
    register char	*cp;
    register unsigned	nchars;		/* number of chars in newstr */
    unsigned		currow;
    unsigned		curcol;
    unsigned		columns;

    vs = curwin->w_vs;
    if (vs->v_insert == NOFUNC) {
	return;
    }

    if (!(echo & e_CHARUPDATE)) {
	return;
    }

    pp = curwin->w_cursor;

    /*
     * If we are at (or near) the end of the line, it's not worth
     * the bother. Define near as 0 or 1 characters to be moved.
     */
    cp = pp->p_line->l_text + pp->p_index;
    if (*cp == '\0' || *(cp+1) == '\0') {
	return;
    }

    curcol = curwin->w_col;

    /*
     * If the cursor is on a longline, and not on the last actual
     * screen line of that longline, we can't do it.
     */
    if (curwin->w_c_line_size > 1 && curcol != curwin->w_virtcol) {
	return;
    }

    nchars = vischar(newchar, &newstr, (int) curcol);

    /*
     * And don't bother if we are (or will be) at the last screen column.
     */
    columns = curwin->w_ncols;
    if (curcol + nchars >= columns) {
	return;
    }

    /*
     * Also, trying to push tabs rightwards doesn't work very
     * well. It's usually better not to use the insert character
     * sequence because in most cases we'll only have to update
     * the line as far as the tab anyway.
     */
    if ((!Pb(P_list) && Pb(P_tabs)) && strchr(cp, '\t') != NULL) {
	return;
    }

    /*
     * Okay, we can do it.
     */
    currow = curwin->w_row;

    VSset_colour(vs, VSCcolour);
    VSinsert(vs, curwin->w_winpos + currow, curcol, newstr);

    /*
     * Update ext_lines.
     */
    {
	register char	*curp;		/* pointer to cursor's char cell */
	unsigned char	*colourp;	/* pointer to cursor's colour cell */
	register char	*last_char;	/* pointer to last char cell in line */
	unsigned char	*last_colour;	/* pointer to last char cell in line */
	register char	*fromcp;
	unsigned char	*fromcolp;

	rp = vs->pv_ext_lines + curwin->w_row;
	rp->s_used += nchars;

	curp = &(rp->s_line[curcol]);
	colourp = &(rp->s_colour[curcol]);
	if (rp->s_used > columns) {
	    rp->s_used = columns;
	}
	last_char = &rp->s_line[rp->s_used];
	*last_char-- = '\0';
	last_colour = &rp->s_colour[rp->s_used - 1];

	/*
	 * Move existing text to the right of the insertion point the
	 * appropriate number of characters to the right, but only if
	 * they are not going to be overwritten by the new stuff.
	 */
	if (last_char - curp >= nchars) {
	    for (fromcp = last_char - nchars, fromcolp = last_colour - nchars; ;
			    last_char--, last_colour--, fromcp--, fromcolp--) {
		*last_char = *fromcp;
		*last_colour = *fromcolp;
		if (fromcp <= curp) {
		    break;
		}
	    }
	}

	/*
	 * This is the string we've just inserted.
	 */
	for (cp = newstr; nchars-- > 0; curp++, colourp++, cp++) {
	    *curp = *cp;
	    *colourp = VSCcolour;
	}
    }
}

void
wind_goto()
{
    VirtScr	*vs;

    if (echo & e_CHARUPDATE) {
	vs = curwin->w_vs;
	VSgoto(vs, (int) curwin->w_winpos + curwin->w_row, curwin->w_col);
    }
}

/*ARGSUSED*/
void
gotocmd(clr)
bool_t	clr;
{
    VirtScr	*vs;

    vs = curwin->w_vs;
    if (clr) {
	VSclear_line(vs, (int) curwin->w_cmdline, 0);
    }
    VSgoto(vs, (int) curwin->w_cmdline, 0);
}

/*
 * Sound the alert.
 */
void
beep()
{
    VSbeep(curwin->w_vs);
}
