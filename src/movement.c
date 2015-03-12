/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    movement.c
* module function:
    Movement of the cursor and the window into the buffer.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

/*
 * Shift a buffer's contents down relative to its window,
 * but don't change the display.
 *
 * Return number of physical lines shifted.
 */
int
shiftdown(win, nlines)
register Xviwin	*win;
unsigned	nlines;
{
    register unsigned	k;		/* loop counter */
    int			done = 0;	/* # of physical lines done */

    for (k = 0; k < nlines; k++) {
	register Line	*p;
	register long	physlines;

	/*
	 * Set the top screen line to the previous one.
	 */
	p = win->w_topline->l_prev;
	if (p == win->w_buffer->b_line0)
	    break;

	physlines = plines(win, p);
	done += LONG2INT(physlines);

	win->w_topline = p;
    }

    return(done);
}

/*
 * Shift a buffer's contents up relative to its window,
 * but don't change the display.
 *
 * Return number of physical lines shifted.
 */
int
shiftup(win, nlines)
register Xviwin	*win;
unsigned	nlines;
{
    register unsigned	k;		/* loop counter */
    int			done = 0;	/* # of physical lines done */

    for (k = 0; k < nlines; k++) {
	register Line	*p;
	register long	physlines;

	/*
	 * Set the top screen line to the next one.
	 */
	p = win->w_topline->l_next;
	if (p == win->w_buffer->b_lastline)
	    break;

	physlines = plines(win, win->w_topline);
	done += LONG2INT(physlines);

	win->w_topline = p;
    }

    return(done);
}

/*
 * Scroll the screen down 'nlines' lines.
 */
void
scrolldown(win, nlines)
register Xviwin	*win;
unsigned	nlines;
{
    s_ins(win, 0, shiftdown(win, nlines));
}

/*
 * Scroll the screen up 'nlines' lines.
 */
void
scrollup(win, nlines)
register Xviwin	*win;
unsigned	nlines;
{
    s_del(win, 0, shiftup(win, nlines));
}

/*
 * xvMoveUp
 * xvMoveDown
 * xvMoveLeft
 * xvMoveRight
 *
 * Move cursor one char {left,right} or one line {up,down}.
 *
 * Return TRUE when successful, FALSE when we hit a boundary
 * (of a line, or the file).
 */

bool_t
xvMoveUp(pp, nlines)
Posn		*pp;
long		nlines;
{
    register Line	*l;
    register long	k;

    for (k = 0, l = pp->p_line; k < nlines && !is_line0(l->l_prev); k++) {
	l = l->l_prev;
    }

    /*
     * If we've at least backed up a little ...
     */
    if (k > 0) {
	pp->p_line = l;
	pp->p_index = 0;
	return(TRUE);
    } else {
	return(FALSE);
    }
}

bool_t
xvMoveDown(pp, nlines)
Posn	*pp;
long	nlines;
{
    register Line	*l;
    register long	k;

    for (k = 0, l = pp->p_line; k < nlines && !is_lastline(l->l_next); k++) {
	l = l->l_next;
    }

    if (k > 0) {
	pp->p_line = l;
	pp->p_index = 0;
	return(TRUE);
    } else {
	return(FALSE);
    }
}

bool_t
one_left(window, unused)
Xviwin	*window;
bool_t	unused;
{
    window->w_set_want_col = TRUE;
    return(xvMoveLeft(window->w_cursor, unused));
}

/*ARGSUSED*/
bool_t
xvMoveLeft(posn, unused)
Posn	*posn;
bool_t	unused;
{
    if (posn->p_index > 0) {
	posn->p_index--;
	return(TRUE);
    } else {
	return(FALSE);
    }
}

/*
 * The move_past_end parameter will be TRUE if moving past the end
 * of the line (onto the first '\0' character) is allowed. We will
 * never move past this character.
 */
bool_t
one_right(window, move_past_end)
Xviwin	*window;
bool_t	move_past_end;
{
    window->w_set_want_col = TRUE;
    return(xvMoveRight(window->w_cursor, move_past_end));
}

bool_t
xvMoveRight(posn, move_past_end)
Posn	*posn;
bool_t	move_past_end;
{
    register char	*txtp;

    txtp = &(posn->p_line->l_text[posn->p_index]);

    if (txtp[0] != '\0' && (move_past_end || txtp[1] != '\0')) {
	posn->p_index++;
	return(TRUE);
    } else {
	return(FALSE);
    }
}

void
begin_line(window, flag)
Xviwin	*window;
bool_t	flag;
{
    xvSetPosnToStartOfLine(window->w_cursor, flag);
    window->w_set_want_col = TRUE;
}

void
xvSetPosnToStartOfLine(pos, flag)
Posn	*pos;
bool_t	flag;
{
    register int	c;

    if (flag) {
	char		*startp;
	register char	*p;

	startp = p = pos->p_line->l_text;
	while ((c = *p) != '\0' && p[1] != '\0' && is_space(c)) {
	    p++;
	}
	pos->p_index = p - startp;
    } else {
	pos->p_index = 0;
    }
}

void
xvMoveToColumn(posn, col)
register Posn	*posn;
register int	col;
{
    register int	c;
    register char	*tstart, *tp;

    tp = tstart = posn->p_line->l_text;
    /*
     * Try to advance to the specified column.
     */
    for (c = 0; c < col; ) {
	if (*tp == '\0') {
	    /*
	     * We're at the end of the line.
	     */
	    break;
	}
	c += vischar(*tp, (char **) NULL, (Pb(P_list) ? -1 : c));

	tp++;
    }

    /*
     * Move back onto last character if we have to.
     */
    if ((*tp == '\0' || c > col) && tp > tstart)
	--tp;
    posn->p_index = tp - tstart;
}

/*
 * Go to the specified line number in the current buffer.
 * Position the cursor at the first non-white.
 */
void
xvMoveToLineNumber(line)
long line;
{
    move_cursor(curwin, gotoline(curbuf, (unsigned long) line), 0);
    begin_line(curwin, TRUE);
}

/*
 * Move the cursor to the specified line, at the specified position.
 */
void
move_cursor(win, lp, index)
Xviwin	*win;
Line	*lp;
int	index;
{
    Posn *p;
    Line *oldline;

    p = win->w_cursor;
    oldline = p->p_line;
    p->p_line = lp;
    p->p_index = index;
    if (oldline != lp)
	info_update(win);
}

/*
 * Adjust window so that currline is, as far as possible, in the
 * middle of it.
 *
 * Don't update the screen: move_window_to_cursor() does that.
 */
static void
jump(win, currline, halfwinsize)
Xviwin		*win;
Line		*currline;
int		halfwinsize;
{
    register int	count;
    register int	spare;
    register Line	*topline;
    register Line	*filestart = win->w_buffer->b_file;

    spare = win->w_nrows - (unsigned int) plines(win, topline = currline) - 1;
    for (count = 0; count < halfwinsize && topline != filestart;) {
	topline = topline->l_prev;
	count += (unsigned int) plines(win, topline);
	if (count >= spare) {
	    if (count > spare)
		topline = topline->l_next;
	    break;
	}
    }
    win->w_topline = topline;
    xvUpdateAllBufferWindows(win->w_buffer);

    /*
     * The result of calling show_file_info here is that if the
     * cursor moves a long away - e.g. for a "G" command or a search
     * - the status line is updated with the correct line number.
     * This is a small cost compared to updating the whole window.
     */
    show_file_info(win, TRUE);
}

/*
 * Update the position of the window relative to the buffer, moving
 * the window if necessary to ensure that the cursor is inside the
 * window boundary. w_topline, w_botline and w_cursor must be set to
 * something reasonable for us to be able to test whether the cursor
 * is in the window or not, unless (as a special case) the buffer is
 * empty - in this case, the cursor sits at top left, despite there
 * being no character there for it to sit on.
 *
 * If we have to move the window only a small amount, we try to scroll
 * it rather than redrawing. If we have to redraw, we also rewrite the
 * status line on the principle that it's probably a negligible cost
 * compared to updating the whole window.
 */
void
move_window_to_cursor(win)
register Xviwin		*win;
{
    register Line	*currline;
    int			halfwinsize;
    long		distance;
    VirtScr		*vsp;
    int			start_row;
    int			end_row;
    Line		*topline;

    currline = win->w_cursor->p_line;
    halfwinsize = (win->w_nrows - 1) / 2;
    vsp = win->w_vs;
    start_row = win->w_winpos;
    end_row = win->w_cmdline - 1;
    topline = win->w_topline;

    /*
     * First stage: move window towards cursor.
     */
    if (bufempty(win->w_buffer)) {
	/*
	 * Special case - file is empty.
	 */
	win->w_topline = win->w_buffer->b_file;
	win->w_cursor->p_line = win->w_buffer->b_file;
	win->w_cursor->p_index = 0;

    } else if (earlier(currline, topline)) {
	long	nlines;

	/*
	 * The cursor is above the top of the window; move the
	 * window towards it.
	 */

	nlines = cntplines(win, currline, topline);

	/*
	 * Decide whether it's worthwhile - & possible - to scroll to
	 * get the window to the right place.
	 *
	 * It's possible if we can scroll part of the screen,
	 * including the status line if necessary.
	 *
	 * If Pn(P_jumpscroll) is js_OFF, we don't use jumpscroll anyway.
	 */
	if (
	    nlines > halfwinsize
	    ||
	    Pn(P_jumpscroll) == js_ON
	    ||
	    (
		Pn(P_jumpscroll) == js_AUTO
		&&
		! VScan_scroll(vsp, start_row, end_row, (int) -nlines)
		&&
		! VScan_scroll(vsp, start_row, end_row + 1, (int) -nlines)
	    )
	) {
	    jump(win, currline, halfwinsize);
	} else {
	    s_ins(win, 0, (int) nlines);
	    win->w_topline = currline;
	    redraw_window(win, FALSE);
	}
    } else {
	long	nlines;

	/*
	 * The cursor is on or after the last line of the screen,
	 * so we might have to move the screen to it. Check to see
	 * whether we are actually off the screen; if not, we don't
	 * have to do anything. We do this in a certain way so as
	 * not to do any unnecessary calculations; this routine is
	 * called very often, so we must not take too much time.
	 */
	if (earlier(currline, win->w_botline->l_prev) ||
			    (plines(win, currline) == 1 &&
			     earlier(currline, win->w_botline))) {
	    return;
	}
	distance = cntplines(win, topline, currline->l_next);
	if (distance <= win->w_nrows - 1) {
	    return;
	}

	/*
	 * The cursor is off the bottom of the window, or the
	 * line the cursor is on won't completely fit in the
	 * window.
	 */

	nlines = distance - (win->w_nrows - 1);

	/*
	 * Decide whether it's worthwhile - & possible - to scroll to
	 * get the window to the right place.
	 *
	 * It's possible if we can scroll part of the screen,
	 * including the status line if necessary.
	 *
	 * If Pn(P_jumpscroll) is js_OFF, we don't use jumpscroll anyway.
	 */
	if (
	    nlines > halfwinsize
	    ||
	    Pn(P_jumpscroll) == js_ON
	    ||
	    (
		Pn(P_jumpscroll) == js_AUTO
		&&
		! VScan_scroll(vsp, start_row, end_row, (int) nlines)
		&&
		! VScan_scroll(vsp, start_row, end_row + 1, (int) nlines)
	    )
	) {
	    jump(win, currline, halfwinsize);
	} else {
	    long	done = 0;
	    Line	*l;
	    Line	*newtopline;

	    /*
	     * Work out where we should place topline in
	     * order that the cursor line is made the
	     * bottom line of the screen.
	     */
	    for (l = currline;; l = l->l_prev) {
		done += plines(win, l);
		if (done >= win->w_nrows - 1)
		    break;
		if (l == win->w_buffer->b_file)
		    break;
	    }
	    while (done > win->w_nrows - 1 && l != currline) {
		done -= plines(win, l);
		l = l->l_next;
	    }
	    newtopline = l;

	    /*
	     * Now work out how many screen lines we want
	     * to scroll the window by. This code assumes
	     * that the old value of topline is earlier
	     * in the buffer than the new, so check this
	     * first.
	     */
	    if (earlier(topline, newtopline)) {
		done = cntplines(win, topline, newtopline);

		if (done != 0) {
		    s_del(win, 0, (int) done);
		}
	    }

	    win->w_topline = newtopline;
	    redraw_window(win, FALSE);
	}
    }
}

/*
 * Make sure the cursor is within the given window.
 *
 * This is needed for commands like control-E & control-Y.
 */
void
move_cursor_to_window(win)
Xviwin		*win;
{
    Posn	*cp;

    cp = win->w_cursor;

    if (earlier(cp->p_line, win->w_topline)) {
	cp->p_line = win->w_topline;
	info_update(win);
	xvMoveToColumn(win->w_cursor, win->w_curswant);
    } else if (!earlier(cp->p_line, win->w_botline)
	       && earlier(win->w_topline, win->w_botline)) {
	cp->p_line = win->w_botline->l_prev;
	info_update(win);
	xvMoveToColumn(win->w_cursor, win->w_curswant);
    }
}
