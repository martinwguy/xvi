/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    windows.c
* module function:
    Window handling functions.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

#undef	min
#define	min(a, b)	(((a) < (b)) ? (a) : (b))

static	bool_t	alloc_window P((Xviwin *));
static	void	dealloc_window P((Xviwin *));
static	Xviwin	*add_window P((Xviwin *, Xviwin *));
static	void	delete_window P((Xviwin *));

/*
 * Initialise the first window. This routine is called only once.
 */
Xviwin *
xvInitWindow(vs)
VirtScr	*vs;
{
    Xviwin	*newwin;

    newwin = add_window((Xviwin *) NULL, (Xviwin *) NULL);
    if (newwin == NULL) {
	return(NULL);
    }

    newwin->w_vs = vs;
    newwin->w_vs->pv_window = newwin;

    if (alloc_window(newwin) == FALSE) {
	free(newwin);
	return(NULL);
    }

    if (!vs_init(vs)) {
	dealloc_window(newwin);
	free(newwin);
	return(NULL);
    }

    newwin->w_winpos = 0;
    newwin->w_nrows = VSrows(vs);
    newwin->w_ncols = VScols(vs);
    newwin->w_cmdline = newwin->w_nrows - 1;

    return(newwin);
}

/*
 * Open a new window by splitting the one given. The size of the
 * old window after the split is determined by sizehint if it is
 * not zero, otherwise the window is just split in half.
 *
 * If a window cannot be opened, a message is displayed and NULL
 * is returned.
 */
Xviwin *
xvOpenWindow(sizehint)
int	sizehint;
{
    Xviwin	*oldwin = curwin;
    Xviwin	*newwin;

    /*
     * Make sure there are enough rows for the new window.
     * This does not obey the minrows parameter, because
     * the point is to have enough space to actually display
     * a window, not just to have a zero-size one.
     */
    if (oldwin->w_nrows < (MINROWS * 2)) {
	/*
	 * Try to grow the current window to make room for the new one.
	 */
	xvResizeWindow((MINROWS * 2) - oldwin->w_nrows);
    }
    if (oldwin->w_nrows < (MINROWS * 2)) {
	show_error("Not enough room!");
	return(NULL);
    }

    newwin = add_window(curwin, oldwin->w_next);
    if (newwin == NULL) {
	show_error("No more windows!");
	return(NULL);
    }

    newwin->w_vs = oldwin->w_vs;
    newwin->w_vs->pv_window = (genptr *) newwin;

    if (alloc_window(newwin) == FALSE) {
	show_error(out_of_memory);
	free((genptr *) newwin);
	return(NULL);
    }

    /*
     * Calculate size and position of new and old windows.
     */
    if (sizehint != 0) {
	newwin->w_nrows = oldwin->w_nrows - sizehint;
    } else {
	newwin->w_nrows = oldwin->w_nrows / 2;
    }
    newwin->w_cmdline = oldwin->w_cmdline;
    newwin->w_winpos = (newwin->w_cmdline - newwin->w_nrows) + 1;

    oldwin->w_nrows -= newwin->w_nrows;
    oldwin->w_cmdline = newwin->w_winpos - 1;

    newwin->w_ncols = oldwin->w_ncols;

    return(newwin);
}

/*
 * Close the current window.
 *
 * Returns a pointer to the new current window or NULL if this was the
 * last window in the current VirstScr..
 */
Xviwin *
xvCloseWindow()
{
    Xviwin	*w1, *w2;
    Xviwin	*new;
    Xviwin	*win = curwin;

    w1 = win->w_last;
    w2 = win->w_next;
    if (w1 == NULL && w2 == NULL) {
	new = NULL;
    } else if (w2 == NULL ||
		(w1 != NULL && w2 != NULL && w1->w_nrows < w2->w_nrows)) {
	w1->w_nrows += win->w_nrows;
	w1->w_cmdline = win->w_cmdline;
	new = w1;
    } else {
	w2->w_nrows += win->w_nrows;
	w2->w_winpos = win->w_winpos;
	new = w2;
    }

    if (new == NULL) {
	/*
	 * Closing last window onto this VirtScr.
	 */
	vs_free(win->w_vs);
	return(NULL);
    } else {
	/*
	 * Be careful not to let the window pointer within
	 * the VirtScr become invalid.
	 */
	win->w_vs->pv_window = (genptr *) new;
	dealloc_window(win);
	delete_window(win);
	return(new);
    }
}

void
xvMapWindowOntoBuffer(w, b)
Xviwin	*w;
Buffer	*b;
{
    /*
     * Connect the two together.
     */
    w->w_buffer = b;
    b->b_nwindows += 1;

    /*
     * Put the cursor and the screen in the right place.
     */
    w->w_cursor->p_line = b->b_file;
    w->w_cursor->p_index = 0;
    w->w_topline = b->b_file;
    w->w_botline = b->b_lastline;

    /*
     * Miscellany.
     */
    w->w_row = w->w_col = 0;
    w->w_virtcol = 0;
    w->w_curswant = 0;
    w->w_set_want_col = FALSE;
}

/*
 * Unmap the current window from its buffer.
 * We don't need to do much here, on the assumption that the
 * calling code is going to do an xvMapWindowOntoBuffer()
 * immediately afterwards; the vital thing is to decrement
 * the window reference count.
 */
void
xvUnMapWindow()
{
    curbuf->b_nwindows -= 1;

    curwin->w_cursor->p_line = NULL;
    curwin->w_topline = NULL;
    curwin->w_botline = NULL;
}

/*
 * Make all windows as nearly the same size as possible.
 * If the nwindows parameter is not zero, it is the number
 * of windows to assume will be present; otherwise, we
 * count the actual number.
 */
void
xvEqualiseWindows(wincount)
int	wincount;
{
    Xviwin	*savecurwin;
    Xviwin	*wp;
    int		winsize;
    int		spare;

    if (curwin == NULL) {
	return;
    }

    /*
     * If wincount is not given, calculate it.
     */
    if (wincount == 0) {
	wp = curwin;
	do {
	    wincount++;
	    wp = xvNextWindow(wp);
	} while (wp != curwin);
    }
    if (wincount == 1) {
	return;
    }

    /*
     * Make 'em all the same, or as close as we can. The top window
     * always gets any spare lines, so it may be bigger than the rest.
     */
    winsize = VSrows(curwin->w_vs) / wincount;
    spare = VSrows(curwin->w_vs) % wincount;

    /*
     * Find the first window in the list.
     */
    for (wp = curwin; wp->w_last != NULL; ) {
	wp = wp->w_last;
    }

    /*
     * Resize all windows except the last one - no point in doing this,
     * as it is a side-effect of resizing all the rest.
     */
    savecurwin = curwin;
    for ( ; wp->w_next != NULL; wp = wp->w_next) {
	set_curwin(wp);
	xvResizeWindow(winsize + spare - wp->w_nrows);
	if (spare > 0) {
	    spare = 0;
	}
    }
    set_curwin(savecurwin);
}

/*
 * Grow or shrink the given buffer window by "nlines" lines.
 * We prefer to move the bottom of the window, and will only
 * move the top when there is no room for manoeuvre below
 * the current one - i.e. any windows are at minimum size.
 */
void
xvResizeWindow(nlines)
int	nlines;
{
    Xviwin	*window = curwin;
    Xviwin	*savecurwin;
    unsigned	savecho;

    if (nlines == 0 || (window->w_last == NULL && window->w_next == NULL)) {
	/*
	 * Nothing to do.
	 */
	return;
    }

    savecurwin = curwin;
    savecho = echo;

    if (nlines < 0) {
	int	spare;		/* num spare lines in this window */

	nlines = - nlines;

	/*
	 * The current window must always contain 2 rows,
	 * so that the cursor has somewhere to go.
	 */
	spare = window->w_nrows - MINROWS;

	/*
	 * If the window is already as small as it
	 * can get, don't bother to do anything.
	 */
	if (spare <= 0)
	    return;

	/*
	 * Don't allow any screen updating until we've
	 * finished moving things around.
	 */
	echo &= ~e_CHARUPDATE;

	/*
	 * First shrink the current window up from the bottom.
	 *
	 * xvMoveStatusLine()'s return value should be negative or 0
	 * in this case.
	 */
	nlines += xvMoveStatusLine(-min(spare, nlines));

	/*
	 * If that wasn't enough, grow the window above us
	 * by the appropriate number of lines.
	 */
	if (nlines > 0) {
	    set_curwin(curwin->w_last);
	    (void) xvMoveStatusLine(nlines);
	}
    } else {
	/*
	 * Don't allow any screen updating until we've
	 * finished moving things around.
	 */
	echo &= ~e_CHARUPDATE;

	/*
	 * Expand window.
	 */
	nlines -= xvMoveStatusLine(nlines);
	if (nlines > 0) {
	    set_curwin(curwin->w_last);
	    (void) xvMoveStatusLine(-nlines);
	}
    }
    set_curwin(savecurwin);

    /*
     * Update screen. Note that status lines have
     * already been updated by xvMoveStatusLine().
     *
     * This still needs a lot more optimization.
     */
    echo = savecho;
    redraw_all(FALSE);
}

/*
 * Adjust the boundary between two adjacent windows by moving the status line
 * up or down, updating parameters for both windows as appropriate.
 *
 * Note that this can shrink the window to size 0.
 */
int
xvMoveStatusLine(nlines)
int	nlines;		/*
			 * number of lines to move (negative for
			 * upward moves, positive for downwards)
			 */
{
    Xviwin	*wp = curwin;	/* Local copy for faster/smaller code */
    Xviwin	*nextwin;

    if (wp == NULL || (nextwin = wp->w_next) == NULL) {
	return(0);
    }

    if (nlines < 0) {		/* move upwards */
	int	amount;
	int	spare;

	amount = -nlines;
	spare = wp->w_nrows - Pn(P_minrows);

	if (amount > spare && wp->w_last != NULL) {
	    /*
	     * Not enough space: call xvMoveStatusLine() recursively
	     * for previous line; note that the second parameter
	     * should be negative.
	     */
	    set_curwin(wp->w_last);
	    (void) xvMoveStatusLine(spare - amount);
	    set_curwin(wp);	/* restore original value */

	    spare = wp->w_nrows - Pn(P_minrows);
	}
	if (amount > spare)
	    amount = spare;
	if (amount != 0) {
	    wp->w_nrows -= amount;
	    wp->w_cmdline -= amount;
	    nextwin->w_winpos -= amount;
	    nextwin->w_nrows += amount;

	    set_curwin(nextwin);
	    (void) shiftdown((unsigned) amount);
	    set_curwin(wp);	/* restore original value */

	    if (wp->w_nrows > 0) {
		show_file_info(TRUE);
	    }
	}
	nlines = -amount;	/* return value */
    } else {			/* move downwards */
	int	spare;

	spare = nextwin->w_nrows - Pn(P_minrows);

	if (nlines > spare) {
	    /*
	     * Not enough space: call xvMoveStatusLine()
	     * recursively for next line.
	     */
	    set_curwin(nextwin);
	    (void) xvMoveStatusLine(nlines - spare);
	    set_curwin(wp);	/* restore original value */
	    spare = nextwin->w_nrows - Pn(P_minrows);
	}
	if (nlines > spare)
	    nlines = spare;
	if (nlines != 0) {
	    wp->w_nrows += nlines;
	    wp->w_cmdline += nlines;
	    nextwin->w_winpos += nlines;
	    nextwin->w_nrows -= nlines;
	    set_curwin(nextwin);
	    (void) shiftup((unsigned) nlines);
	    set_curwin(wp);	/* restore original value */
	    if (wp->w_nrows > 0) {
		show_file_info(TRUE);
	    }
	}
    }
    return(nlines);
}

/*
 * The given VirtScr has just been resized. Adjust the sizes of the
 * windows using that VirtScr to fit into the new screen size.
 *
 * The "curwin" pointer is changed if necessary, to avoid pointing
 * to a window that has been reduced to zero size.
 */
void
xvAdjustWindows(vs)
VirtScr	*vs;
{
    Xviwin	*first_win;
    Xviwin	*last_win;
    Xviwin	*w;
    int		totlines;
    int		to_go;		/* How many lines we need to save or to fill */
    int		nw;		/* Total number if windows in this VirtScr */
    int		ndw;		/* Number of displayed windows */
    int		last_winpos, last_nrows;
    int		minrows = Pn(P_minrows);

    /*
     * Find the first and last windows onto this VirtScr.
     */
    for (w = (Xviwin *) vs->pv_window; w->w_last != NULL; w = w->w_last) {
	;
    }

    first_win = w;

    for ( ; w->w_next != NULL; w = w->w_next) {
	;
    }
    last_win = w;

    if (first_win == last_win) {
	/* And both are == curwin */
	first_win->w_nrows = VSrows(vs);
	first_win->w_ncols = VScols(vs);
	first_win->w_cmdline = first_win->w_nrows + first_win->w_winpos - 1;
	redraw_all(TRUE);
	return;
    }

    /*
     * Count the total line usage, and at the same time
     * go to the bottom-most window onto the VirtScr.
     */
    totlines = nw = ndw = 0;
    for (w = first_win; w != NULL; w = w->w_next) {
	totlines += w->w_nrows;
	nw++;
	if (w->w_nrows > 0) ndw++;
    }
    if (totlines <= VSrows(vs)) {
	/*
	 * VirtScr has got longer.
	 *
	 * First, we need to check for undisplayed windows
	 * and redisplay them if feasible.
	 *
	 * w is the window we are thinking of redisplaying.
	 * to_go is the number of screen lines not used by any window.
	 */
	to_go = VSrows(vs) - totlines;
	for (w = first_win; w != NULL && to_go > 0; w = w->w_next) {
	    if (w->w_nrows < minrows) {
		/* This window is undisplayed */
		int inc = to_go > minrows ? minrows : to_go;
		if (w->w_nrows + inc >= minrows) {
		    w->w_nrows += inc;
		    to_go -= inc;
		    ndw++;
		} else {
		    /* Not enough room for this window in just the new space.
		     * See whether the above windows can donate enough lines
		     * to be able to redisplay it.
		     *
		     * "navail" is the number of free lines we could get if we
		     *		were to shrink every displayed window to minrows
		     *		"navail" includes the lines counted by to_go.
		     * "need"	is how many lines we need to steal from other
		     *		windows to be able to redisplay window w.
		     */
		    int		navail	= VSrows(vs) - ndw * minrows;
		    int		need	= minrows;
		    if (navail >= need) {
			/*
			 * It can be done. Steal lines from the above windows
			 * until we have enough to redisplay window w.
			 */
			Xviwin	*wup;	/* An upper window */

			/* First, give it the unused screen line(s) */
			w->w_nrows += to_go;
			to_go = 0;
			/* Then go stealing lines from the other windows */
			for (wup = w->w_last; wup != NULL; wup = wup->w_last) {
			    /* How many rows could be taken from this window? */
			    int take;
			    take = wup->w_nrows - minrows;
			    /* Only take what we need */
			    if (need < take) take = need;
			    if (take > 0) {
				wup->w_nrows -= take;
				w->w_nrows += take;
				need -= take;
				if (w->w_nrows >= minrows)
				    break;
			    }
			}
			if (w->w_nrows < minrows) abort();
		    }
		}
	    }
	}
    } else {
	int	spare;
	/*
	 * VirtScr has got smaller. Shrink each window in turn from
	 * the bottom up until we have freed up enough lines to make
	 * all the windows fit.
	 *
	 * "spare" is how many free lines there would be if we resized
	 *	   all the displayed windows to minimum size.
	 */
	spare = VSrows(vs) - minrows * ndw;
	to_go = totlines - VSrows(vs);

	for (w = last_win; w != NULL && to_go > 0; w = w->w_last) {
	    if (w->w_nrows == 0) {
		continue;
	    }
	    if (to_go <= w->w_nrows - minrows) {
		/*
		 * Just have to reduce this window a bit.
		 */
		w->w_nrows -= to_go;
		to_go = 0;
	    } else if (to_go > spare) {
		/*
		 * There is not enough room to keep this window displayed.
		 */
		to_go -= w->w_nrows;
		spare += minrows;
		w->w_nrows = 0;
		/*
		 * If we undisplayed the current window, move to another.
		 * We always undisplay the last windows first, so look back
		 * up the list of windows for a previous one that is displayed.
		 */
		if (curwin == w) {
		    /* Search back up the list for a displayed window */
		    for (w = curwin->w_last; w != NULL; w = w->w_last) {
			if (w->w_nrows != 0) {
			    set_curwin(w);
			    break;
			}
					}
		    /* Not found? Search forward */
		    if (w == NULL)
		    for (w = curwin->w_next; w != NULL; w = w->w_next) {
			if (w->w_nrows != 0) {
			    set_curwin(w);
			    break;
			}
		    }
		    if (w == NULL) abort();
		}
	    } else {
		/*
		 * Room enough for this window at minimum size.
		 */
		to_go -= w->w_nrows - minrows;
		spare -= w->w_nrows - minrows;
		w->w_nrows = minrows;
	    }
	}
    }

    /*
     * Add remaining lines to bottom displayed window.
     *
     * to_go is left negative if we undisplayed a window because
     * the space left for it was less than minrows but more than zero.
     */
    if (to_go < 0) to_go = -to_go;
    if (to_go > 0) {
	/* Find the bottommost displayed window */
	for (w=last_win; w != NULL && w->w_nrows == 0; w=w->w_last)
	    ;
	if (w == NULL) abort();
	w->w_nrows += to_go;
    }

    /*
     * Adjust position of all the windows.
     */
    last_winpos = last_nrows = 0;
    for (w = first_win; w != NULL; w = w->w_next) {
	if (w->w_nrows != 0) {
	    w->w_winpos = last_winpos + last_nrows;
	    w->w_cmdline = w->w_winpos + w->w_nrows - 1;
	    last_winpos = w->w_winpos;
	    last_nrows = w->w_nrows;
	} else {
	    w->w_winpos = 0;
	    w->w_cmdline = 0;
	}
    }

    /*
     * Adjust the number of columns.
     */
    for (w = first_win; w != NULL; w = w->w_next) {
	w->w_ncols = VScols(vs);
    }

    redraw_all(TRUE);
}

/*
 * Use the specified window.
 */
void
xvUseWindow()
{
    curwin->w_vs->pv_window = (genptr *) curwin;
}

/*
 * Update all windows associated with the current buffer.
 */
void
xvUpdateAllBufferWindows()
{
    Buffer	*buffer = curbuf;
    Xviwin	*savecurwin = curwin;

    do {
	if (curbuf == buffer) {
	    redraw_window(FALSE);
	}
	set_curwin(xvNextDisplayedWindow(curwin));
    } while (curwin != savecurwin);
}

/***************************************************************************
 *                                                                         *
 * Window allocation routines - deal with allocation of data structures    *
 * within a window, once the window itself has been created, and with      *
 * the freeing up of data structures before a window is deleted.           *
 *                                                                         *
 ***************************************************************************/

/*
 * Set up and allocate data structures for the given window,
 * assumed to contain a valid VirtScr pointer.
 */
static bool_t
alloc_window(win)
Xviwin	*win;
{
    /*
     * Allocate space for the status line.
     */
    flexnew(&win->w_statusline);

    /*
     * Allocate a Posn structure for the cursor.
     */
    win->w_cursor = alloc(sizeof(Posn));
    if (win->w_cursor == NULL) {
	return(FALSE);
    }

    win->w_cmd = alloc(sizeof(Cmd));
    if (win->w_cmd == NULL) {
	return(FALSE);
    }
    xvInitialiseCmd(win->w_cmd);

    return(TRUE);
}

/*
 * Free up all the space used within the given window,
 * i.e. that which was allocated by alloc_window().
 */
static void
dealloc_window(win)
Xviwin	*win;
{
    free(win->w_cursor);
    free(win->w_cmd);
    flexdelete(&win->w_statusline);
}

/***************************************************************************
 *                                                                         *
 * Window list routines - deal with getting new windows and adding them    *
 * into the list, and removing windows from the list when they are closed. *
 * Also routines to get the next window, or find one with a matching name. *
 *                                                                         *
 ***************************************************************************/

/*
 * Allocate a new window.
 */
static Xviwin *
add_window(last, next)
Xviwin	*last, *next;
{
    Xviwin	*newwin;

    newwin = alloc(sizeof(Xviwin));
    if (newwin == NULL) {
	return(NULL);
    }

    /*
     * Link the window into the list.
     */
    if (last != NULL) {
	last->w_next = newwin;
    }
    if (next != NULL) {
	next->w_last = newwin;
    }
    newwin->w_last = last;
    newwin->w_next = next;

    return(newwin);
}

/*
 * Delete the given window from the list, freeing up its memory.
 * This is the opposite of add_window().
 */
static void
delete_window(window)
Xviwin	*window;
{
    if (window == NULL)
	return;

    if (window->w_next != NULL) {
	window->w_next->w_last = window->w_last;
    }
    if (window->w_last != NULL) {
	window->w_last->w_next = window->w_next;
    }

    window->w_buffer->b_nwindows -= 1;

    free((genptr *) window);
}

/*
 * Given a window, find the "next" one in the list.
 */
Xviwin *
xvNextWindow(window)
Xviwin	*window;
{
    if (window == NULL) {
	return(NULL);
    } else if (window->w_next != NULL) {
	return(window->w_next);
    } else {
	Xviwin	*tmp;

	/*
	 * No next window; go to start of list.
	 */
	for (tmp = window; tmp->w_last != NULL; tmp = tmp->w_last)
	    ;
	return(tmp);
    }
}

/*
 * Given a window, find the next displayed window in the list.
 * If there are no other displayed windows, the passed window is returned.
 */
Xviwin *
xvNextDisplayedWindow(window)
Xviwin	*window;
{
    Xviwin	*wp;

    if (window == NULL) {
	return(NULL);
    }

    for (wp = xvNextWindow(window); wp != window && wp != NULL;
		wp = xvNextWindow(wp)) {
	if (wp->w_nrows > 0) {
	    break;
	}
    }
    return(wp);
}

/*
 * Find the next window onto the buffer with the given filename,
 * starting at the current one; if there isn't one, or if it is
 * too small to move into, return NULL.
 */
Xviwin *
xvFindWindowByName(window, filename)
Xviwin	*window;
char	*filename;
{
    Xviwin	*wp;
    char	*f;

    if (window != NULL && filename != NULL) {
	wp = window;
	do {
	    f = wp->w_buffer->b_filename;
	    if (f != NULL && strcmp(filename, f) == 0) {
		return(wp);
	    }
	    wp = xvNextDisplayedWindow(wp);
	} while (wp != window);
    }

    return(NULL);
}

bool_t
xvCanSplit()
{
    Xviwin	*wp;
    int		nw;

    wp = curwin;
    nw = 0;
    do {
	wp = xvNextWindow(wp);
	nw++;
    } while (wp != curwin);
    return(nw < Pn(P_autosplit));
}
