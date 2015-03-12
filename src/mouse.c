/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    mouse.c
* module function:
    Machine-independent mouse interface routines.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

/*
 * Find the Line corresponding to a given physical screen row.
 *
 * Also return (in *pstartrow) the physical screen row on which that
 * Line starts.
 */
static Line *
findline(wp, row, pstartrow)
Xviwin		*wp;
register int	row;
int		*pstartrow;
{
    register int	lposn;
    int			maxrow;
    register Line	*lp;

    lp = wp->w_topline;
    lposn = wp->w_winpos;
    maxrow = wp->w_cmdline;

    for (;;) {
	register int	newposn;

	newposn = lposn + LONG2INT(plines(wp, lp));
	if (
	    (
		/*
		 * If we've found the right line ...
		 */
		row >= lposn
		&&
		row < newposn
	    )
	    ||
	    (
		/*
		 * ... or we've got to the end of the buffer ...
		 */
		lp->l_next == wp->w_buffer->b_lastline
	    )
	    ||
	    (
		/*
		 * ... or we're at the bottom of the window ...
		 */
		newposn >= maxrow
	    )
	) {
	    /*
	     * ... we return this line.
	     */
	    *pstartrow = lposn;
	    return lp;
	}
	lposn = newposn;
	lp = lp->l_next;
    }
}

/*
 * Move the text cursor as near as possible to the given physical
 * screen co-ordinates.
 */
static void
setcursor(wp, row, col)
Xviwin	*wp;
int	row;		/* row where mouse was clicked */
int	col;		/* column where mouse was clicked */
{
    int		startrow;	/* row at which line starts */
    int		vcol;		/* virtual column */
    Line	*lp;		/* logical line corresponding to row */

    lp = findline(wp, row, &startrow);
    vcol = col + ((row - startrow) * wp->w_ncols);
    move_cursor(wp, lp, 0);
    xvMoveToColumn(wp->w_cursor, (wp->w_curswant = vcol));
}

/*
 * Find the window a given physical screen row is in.
 */
static Xviwin *
findwin(row)
register int	row;
{
    register Xviwin	*wp;

    wp = curwin;
    for (;;) {
	if (wp->w_winpos <= row && row <= wp->w_cmdline) {
	    /*
	     * We've found the right window.
	     */
	    return wp;
	}
	if ((wp = xvNextDisplayedWindow(wp)) == curwin) {
	    /*
	     * This can't happen: the mouse isn't in any
	     * window.
	     */
	    return NULL;
	}
    }
}

/*
 * Handle mouse drag event.
 */
void
mousedrag(row1, row2, col1, col2)
int row1, row2, col1, col2;
{
    if (State == NORMAL && row1 != row2 && row1 < VSrows(curwin->w_vs) - 1) {
	Xviwin *wp;

	if ((wp = findwin(row1)) == NULL) {
	    /*
	     * This can't happen.
	     */
	    return;
	}

	if (wp->w_cmdline == row1) {
	    unsigned savecho;

	    savecho = echo;
	    echo &= ~e_CHARUPDATE;

	    (void) xvMoveStatusLine(wp, row2 - row1);

	    echo = savecho;

	    move_cursor_to_window(curwin);
	    redraw_all(curwin, FALSE);
	    cursupdate(curwin);
	    wind_goto(curwin);
	}
    }
}

/*
 * This function is called by the (obviously machine-dependent) mouse
 * event handling code when a mouse button is pressed & released.
 *
 * The algorithm we use here is semantically complicated, but seems to
 * be fairly intuitive in actual use.
 */
void
mouseclick(row, col)
register int	row;	/* row the mouse cursor is in */
int		col;	/* column the mouse cursor is in */
{
    register Xviwin	*wp;

    if (State != NORMAL) {
	return;
    }

    /*
     * First find which window the mouse is in.
     */
    if ((wp = findwin(row)) == NULL) {
	/*
	 * This can't happen.
	 */
	return;
    }

    if (wp != curwin) {
	/*
	 * The window the mouse is in isn't the current window.
	 * Make it the current window.
	 */
	curwin = wp;
	curbuf = wp->w_buffer;
    }

    if (row == wp->w_cmdline) {
	/*
	 * If the mouse is on the status line of any window,
	 * we update the information on the status line. This
	 * applies whether or not it was already the current
	 * window.
	 */
	show_file_info(wp, TRUE);
    } else {
	/*
	 * Move the cursor as near as possible to where the
	 * mouse was clicked.
	 */
	setcursor(wp, row, col);

	/*
	 * If the window has at least 2 text rows, and ...
	 */
	if (wp->w_nrows > 2) {
	    if (row == wp->w_winpos) {
		/*
		 * ... we're on the top line of the
		 * window - scroll down ...
		 */
		scrolldown(wp, (unsigned) 1);
		redraw_window(wp, FALSE);
	    } else if (row == wp->w_cmdline - 1) {
		/*
		 * ... or we're on the bottom line of
		 * the window - scroll up ...
		 */
		scrollup(wp, (unsigned) 1);
		redraw_window(wp, FALSE);
	    }
	}
    }

    /*
     * Make sure physical screen and cursor position are updated.
     */
    cursupdate(wp);
    wind_goto(wp);
}

/*
 * Handle the mouse moving to a new location with no buttons pressed.
 * We set the current window to the one under ypos (if it is different
 * from the existing current window).
 */
void
mousemove(ypos)
int ypos;
{
    Xviwin	*wp;

    if (State != NORMAL) {
	return;
    }

    if ((wp = findwin(ypos)) == NULL || wp == curwin) {
	return;
    }

    show_file_info(curwin, TRUE);

    curwin = wp;
    curbuf = wp->w_buffer;
    cursupdate(wp);
    wind_goto(wp);
}
