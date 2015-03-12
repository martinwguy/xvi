/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    cursor.c
* module function:
    Deal with movement of the screen cursor - i.e. when the
    logical cursor moves within a buffer, work out the new
    position of the screen cursor within its window.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

static	void	calc_position_in_line P((Xviwin *));

/*
 * Update the window's variables which say where the cursor is.
 * These are row, col and virtcol, curswant if w_set_want_col.
 *
 * We also update w_c_line_size, which is used in screen.c to
 * figure out whether the cursor line has changed size or not.
 */
void
cursupdate(win)
Xviwin	*win;
{
    /*
     * Calculate physical lines from logical lines.
     */
    win->w_row = cntplines(win, win->w_topline, win->w_cursor->p_line);
    win->w_c_line_size = plines(win, win->w_cursor->p_line);

    /*
     * Calculate new position within the line.
     */
    calc_position_in_line(win);
}

static void
calc_position_in_line(win)
Xviwin	*win;
{
    register int	i;
    register char	*cltp;
    register int	ccol;
    register unsigned	width;
    register int	index;
    register int	c;
    register Posn	*curp;

    curp = win->w_cursor;
    cltp = curp->p_line->l_text;
    index = curp->p_index;
    ccol = Pb(P_number) ? NUM_SIZE : 0;

    /*
     * Work out the virtual column within the current line.
     *
     * This loop needs to be as fast as possible; it gets called very
     * often.
     */
    for (i = width = 0; i <= index;) {
	ccol += width;
	width = vischar((c = cltp[i++]), (char **) NULL, ccol);
    }

    if (State != INSERT && c == '\t' && ! Pb(P_list) && Pb(P_tabs)) {
	/*
	 * If we are inserting, or we're on a control
	 * character other than a tab, or we aren't showing
	 * tabs normally, the cursor goes to the first column
	 * of the control character representation: otherwise,
	 * if it's a tab & we aren't in insert mode, we place
	 * the cursor on the last column of the tab
	 * representation. (This is like the "real" vi.)
	 */
	ccol += width - 1;
    }

    /*
     * If showing line numbers, adjust the virtual column within the line.
     */
    win->w_virtcol = ccol - (Pb(P_number) ? NUM_SIZE : 0);

    /*
     * Convert virtual column to screen column by line folding.
     */
    while (ccol >= win->w_ncols) {
	win->w_row++;
	ccol -= win->w_ncols;
    }
    win->w_col = ccol;

    /*
     * Don't go past bottom line of window. (This should only
     * happen on a longline which is too big to fit in the
     * window.)
     */
    if (win->w_row >= win->w_nrows - 1) {
	win->w_row = win->w_nrows - 2;
    }

    if (win->w_set_want_col) {
	win->w_curswant = win->w_virtcol;
	win->w_set_want_col = FALSE;
    }
}
