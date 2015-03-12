/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    undo.c
* module function:
    Code to implement "undo" command.
* usage:
    We provide several primitive functions for "do"ing things,
    and an "undo" function to restore the previous state.

    Normally, a primitive function is simply called, and will
    automatically throw away the old previous state before
    saving the current one and then making the change.

    Alternatively, it is possible to bracket lots of changes
    between calls to the start_command() and end_command()
    functions; more global changes can then be effected,
    and undo still works as it should.  This is used for the
    "global" command, for multi-line substitutes, and for
    insert mode.

    If a command fails, we should really back out of the whole
    transaction, undoing any changes which have been done.
    That we do not is not particularly harmful - we just leave
    it to the user to do it instead. It would be better if we
    did it automatically.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"
#include "change.h"

static	void	save_position P((Xviwin *));
static	bool_t	init_change_data P((Xviwin *));
static	void	push_change_list P((ChangeStack *));
static	bool_t	pop_change_list P((ChangeStack *, Change **));
static	void	free_changes P((Change *));
static	Change	*_replchars P((Xviwin *, Line *, int, int, char *));
static	Change	*_repllines P((Xviwin *, Line *, long, Line *));
static	void	report P((Xviwin *));

void
init_undo(buffer)
Buffer	*buffer;
{
    ChangeData	*cdp;

    /*
     * Initialise the undo-related variables in the Buffer.
     */
    cdp = (ChangeData *) alloc(sizeof(ChangeData));
    if (cdp == NULL) {
    	return;
    }
    cdp->cd_nlevels = 0;

    cdp->cd_undo = (ChangeStack *) alloc(sizeof(ChangeStack));
    if (cdp->cd_undo == NULL) {
	return;
    }
    cdp->cd_undo->cs_stack[0] = NULL;
    cdp->cd_undo->cs_size = 0;

    cdp->cd_redo = (ChangeStack *) alloc(sizeof(ChangeStack));
    if (cdp->cd_redo == NULL) {
	return;
    }
    cdp->cd_redo->cs_stack[0] = NULL;
    cdp->cd_redo->cs_size = 0;

    buffer->b_change = (genptr *) cdp;

    /*
     * Set line numbers.
     */
    buffer->b_line0->l_number = 0;
    buffer->b_file->l_number = 1;
    buffer->b_lastline->l_number = MAX_LINENO;
}

/*
 * Free memory associated with the change structures for a buffer.
 */
void
free_undo(buffer)
Buffer	*buffer;
{
    ChangeData	*cdp;
    Change	*chp;

    cdp = (ChangeData *) buffer->b_change;

    /*
     * Remove all recorded changes for this buffer.
     */
    while (pop_change_list(cdp->cd_undo, &chp)) {
	free_changes(chp);
    }
    while (pop_change_list(cdp->cd_redo, &chp)) {
	free_changes(chp);
    }

    free(buffer->b_change);
}

/*
 * Push a new change list onto the stack. We always start with
 * a NULL list, because we haven't actually changed anything yet.
 */
static void
push_change_list(csp)
ChangeStack	*csp;
{
    int		i;

    /*
     * Discard any changes if we have reached our limit.
     */
    while (csp->cs_size >= Pn(P_undolevels)) {
	free_changes(csp->cs_stack[csp->cs_size - 1]);
	csp->cs_size -= 1;
    }

    /*
     * Push the previous elements down the stack to make room.
     */
    for (i = csp->cs_size - 1; i >= 0; --i) {
	csp->cs_stack[i + 1] = csp->cs_stack[i];
    }

    csp->cs_stack[0] = NULL;
    csp->cs_size++;
}

static bool_t
pop_change_list(csp, chpp)
ChangeStack	*csp;
Change		**chpp;
{
    int			i;

    if (csp->cs_size == 0) {
	return(FALSE);
    }

    /*
     * Copy the top element off the stack.
     */
    *chpp = csp->cs_stack[0];

    /*
     * Copy the previous elements back up the stack.
     */
    csp->cs_size -= 1;
    for (i = 0; i < csp->cs_size; i++) {
	csp->cs_stack[i] = csp->cs_stack[i + 1];
    }

    return(TRUE);
}

static void
push_change(csp, change)
ChangeStack	*csp;
Change		*change;
{
    change->c_next = csp->cs_stack[0];
    csp->cs_stack[0] = change;
}

/*
 * Start a command. This routine may be called many times;
 * what it does is increase the variable which shows how
 * many times it has been called.  The end_command() then
 * decrements the variable - so it is vital that calls
 * of the two routines are matched.
 *
 * The effect of this is quite simple; if the nlevels variable
 * is >0, the "do*" routines will not throw away the previous
 * state before making a change; and thus we are able to undo
 * multiple changes.
 *
 * All the do* routines, and start_command(), will throw away
 * the previous saved state if the cd_nlevels variable is 0.
 */
bool_t
start_command(window)
Xviwin	*window;
{
    ChangeData	*cdp = (ChangeData *) window->w_buffer->b_change;

    if (!init_change_data(window)) {
	return(FALSE);
    }

    cdp->cd_nlevels += 1;

    return(TRUE);
}

static bool_t
init_change_data(window)
Xviwin	*window;
{
    ChangeData	*cdp = (ChangeData *) window->w_buffer->b_change;
    Change	*chp;

    if (cdp->cd_nlevels == 0) {
	if (not_editable(window->w_buffer)) {
	    show_error(window, "Edit not allowed!");
	    return(FALSE);
	}

	/*
	 * We are making a change to the buffer, so we have to ensure
	 * that the redo stack is empty.
	 */

	while (pop_change_list(cdp->cd_redo, &chp)) {
	    free_changes(chp);
	}

	push_change_list(cdp->cd_undo);
	cdp->cd_total_lines = 0;
	cdp->cd_nlevels = 0;
	save_position(window);
    }
    return(TRUE);
}

/*
 * Save the cursor position.
 *
 * This is called at the start of each change, so that
 * the cursor will return to the right place after an undo.
 */
static void
save_position(window)
Xviwin	*window;
{
    register Change	*change;
    register ChangeData	*cdp;

    cdp = (ChangeData *) window->w_buffer->b_change;

    change = challoc();
    if (change == NULL) {
	return;
    }

    change->c_type = C_POSITION;
    change->c_pline = lineno(window->w_buffer, window->w_cursor->p_line);
    change->c_pindex = window->w_cursor->p_index;

    push_change(cdp->cd_undo, change);
}

void
end_command(window)
Xviwin	*window;
{
    register ChangeData	*cdp = (ChangeData *) window->w_buffer->b_change;

    if (cdp->cd_nlevels > 0) {
	cdp->cd_nlevels -= 1;
	if (cdp->cd_nlevels == 0) {
	    report(window);
	}
    } else {
	show_error(window, "Internal error: too many \"end_command\"s");
    }
}

/*
 * Interface used by rest of editor code to _replchars(). We do two
 * extra things here: call init_change_data() and push the anti-change
 * onto the undo stack. These are only valid when we are making a real
 * change; _repllines() gets called when we are undoing or redoing a
 * change, when we don't want those things to happen.
 */
void
replchars(window, line, start, nchars, newstring)
Xviwin	*window;
Line	*line;
int	start;
int	nchars;
char	*newstring;
{
    ChangeData	*cdp = (ChangeData *) window->w_buffer->b_change;
    Change	*change;

    if (!init_change_data(window)) {
	return;
    }

    change = _replchars(window, line, start, nchars, newstring);

    /*
     * Push this change onto the LIFO of changes
     * that form the current command.
     */
    if (change != NULL) {
	push_change(cdp->cd_undo, change);
    }
}

/*
 * Interface used by rest of editor code to _repllines(). We do two
 * extra things here: call init_change_data() and push the anti-change
 * onto the undo stack. These are only valid when we are making a real
 * change; _repllines() gets called when we are undoing or redoing a
 * change, when we don't want those things to happen.
 */
void
repllines(window, line, nolines, newlines)
register Xviwin	*window;
Line		*line;
long		nolines;
Line		*newlines;
{
    ChangeData	*cdp = (ChangeData *) window->w_buffer->b_change;
    Change	*change;

    if (!init_change_data(window)) {
	return;
    }

    change = _repllines(window, line, nolines, newlines);

    /*
     * Push this change onto the LIFO of changes
     * that form the current command.
     */
    if (change != NULL) {
	push_change(cdp->cd_undo, change);
    }

    if (cdp->cd_nlevels == 0) {
	report(window);
    }
}

/*
 * Replace the given section of the given line with the
 * new (null-terminated) string. nchars may be zero for
 * insertions of text; newstring may point to a 0-length
 * string to delete text. start is the first character
 * of the section which is to be replaced.
 *
 * Note that we don't call strcpy() to copy text here, for
 * two reasons: firstly, we are likely to be copying small
 * numbers of characters and it is therefore faster to do
 * the copying ourselves, and secondly, strcpy() doesn't
 * necessarily work for copying overlapping text towards
 * higher locations in memory.
 */
static Change *
_replchars(window, line, start, nchars, newstring)
Xviwin	*window;
Line	*line;
int	start;
int	nchars;
char	*newstring;
{
    register char	*from;		/* where to copy from */
    register char	*to;		/* where to copy to */
    register int	nlen;		/* length of newstring */
    register int	olen;		/* length of old line */
    register int	offset;		/* how much to move text by */
    Buffer		*buffer;
    Change		*change;

    buffer = window->w_buffer;

    /*
     * First thing we have to do is to obtain a change
     * structure to record the change so we can be sure
     * that we can undo it. If this fails, we must not
     * make any change at all.
     */
    change = challoc();
    if (change == NULL) {
	return(NULL);
    }

    nlen = strlen(newstring);
    olen = strlen(line->l_text + start);
    if (olen < nchars)
	nchars = olen;
    offset = nlen - nchars;

    /*
     * Record the opposite change so we can undo it.
     */
    if (nchars == 0) {
	change->c_type = C_DEL_CHAR;
    } else {
	change->c_type = C_CHAR;
	change->c_chars = alloc((unsigned) nchars + 1);
	if (change->c_chars == NULL) {
	    chfree(change);
	    State = NORMAL;
	    return(NULL);
	}
	(void) strncpy(change->c_chars, line->l_text + start, nchars);
	change->c_chars[nchars] = '\0';
    }
    change->c_lineno = lineno(buffer, line);
    change->c_index = start;
    change->c_nchars = nlen;

    if (offset > 0) {
	register char	*s;

	/*
	 * Move existing text along by offset to the right.
	 * First make some room in the line.
	 */
	if (grow_line(line, offset) == FALSE) {
	    free(change->c_chars);
	    chfree(change);
	    State = NORMAL;
	    return(NULL);
	}

	/*
	 * Copy characters backwards, i.e. start
	 * at the end of the line rather than at
	 * the start.
	 */
	from = line->l_text + start;
	to = from + offset + olen + 1;
	s = from + olen + 1;
	while (s > from) {
	    *--to = *--s;
	}

    } else if (offset < 0) {

	/*
	 * Move existing text along to the left.
	 */
	offset = - offset;
	to = line->l_text + start;
	from = to + offset;

	/*
	 * Do classic K&R strcpy().
	 */
	while ((*to++ = *from++) != '\0') {
	    ;
	}
    }

    /*
     * Finally, copy the new text into position.
     * Note that we are careful not to copy the
     * null terminator.
     */
    from = newstring;
    to = line->l_text + start;
    while (*from != '\0') {
	*to++ = *from++;
    }

    buffer->b_flags |= FL_MODIFIED;

    return(change);
}

/*
 * Replace the specified set of lines with the replacement set.
 * The number of lines to be replaced may be 0; the replacement
 * list may be a NULL pointer.
 */
static Change *
_repllines(window, line, nolines, newlines)
register Xviwin	*window;
Line		*line;
long		nolines;
Line		*newlines;
{
    register Buffer	*buffer;	/* buffer window is mapped onto */
    Line		*firstp;	/* line before first to delete */
    Line		*lastp;		/* line after last to delete */
    Line		*lastline;	/* last line to delete */
    Line		*new_start;	/* start of lines to be inserted */
    Line		*new_end;	/* last line to be inserted */
    long		nnlines;	/* no. new logical lines */
    long		oplines;	/* no. physical lines to be replaced */
    long		nplines;	/* no. new physical lines */
    long		n;		/* lines done so far */
    register Xviwin	*wp;		/* loop variable */
    Change		*change;
    register ChangeData	*cdp;

    buffer = window->w_buffer;
    cdp = (ChangeData *) buffer->b_change;

    /*
     * First thing we have to do is to obtain a change
     * structure to record the change so we can be sure
     * that we can undo it. If this fails, we must not
     * make any change at all.
     */
    change = challoc();
    if (change == NULL) {
	return(NULL);
    }
    change->c_type = C_LINE;

    /*
     * Work out how many lines are in the new set, and set up
     * pointers to the beginning and end of the list.
     */
    if (newlines != NULL) {
	/*
	 * We have a new set of lines to replace the old.
	 */
	new_start = newlines;
	nnlines = 1;
	nplines = 0;
	for (new_end = newlines; new_end->l_next != NULL;
					    new_end = new_end->l_next) {
	    nnlines++;
	    nplines += plines(window, new_end);
	}
	nplines += plines(window, new_end);
    } else {
	/*
	 * No new lines; we are just deleting some.
	 */
	new_start = new_end = NULL;
	nnlines = 0;
	nplines = 0;
    }

    /*
     * Point "firstp" at the line before the first to be deleted,
     * "lastline" at the last line to be deleted, and "lastp" at
     * the line after the last to be deleted. To do the latter,
     * we must loop through the buffer from the specified start
     * line "nolines" times. We also use this loop to set up the
     * "oplines" variable.
     *
     * The line number recorded for the change is the number of the
     * line immediately before it, plus 1. This copes with line being
     * equal to the lastline pointer, which is sometimes necessary
     * for deleting lines at the end of the file.
     */
    firstp = line->l_prev;
    lastline = line;
    change->c_lineno = firstp->l_number + 1;

    oplines = 0;
    for (lastp = line, n = 0; lastp != buffer->b_lastline && n < nolines;
					    n++, lastp = lastp->l_next) {

	lastline = lastp;

	/*
	 * Clear any marks that may have been set
	 * on the lines to be deleted.
	 */
	clrmark(lastp, buffer);

	oplines += plines(window, lastp);

	/*
	 * Scan through all windows which are mapped
	 * onto the buffer we are modifying, to see
	 * if we need to update their w_topline and/or
	 * w_cursor elements as a result of the change.
	 *
	 * There is a disgusting hack here. If the number
	 * of lines being added/deleted is such that the
	 * cursor could remain at the same logical line
	 * in the file after the change, then it should.
	 * Since the cursor could be in different places
	 * in each window, we use the Posn.p_index field
	 * to store the offset from the start of the
	 * changed section. This is only used if the
	 * Posn.p_line field has been set to NULL.
	 */
	wp = window;
	do {
	    if (wp->w_buffer != buffer)
		continue;

	    if (lastp == wp->w_cursor->p_line) {
		long	offset;

		/*
		 * Mark the cursor line as NULL
		 * so we will know to re-assign
		 * it later.
		 */
		wp->w_cursor->p_line = NULL;
		offset = cntllines(line, lastp);
		if (offset > INT_MAX) {
		    offset = 0;
		}
		wp->w_cursor->p_index = offset;
	    }
	} while ((wp = xvNextWindow(wp)) != window);
    }

    /*
     * Hack.
     *
     * If we are replacing the entire buffer with no lines, we must
     * insert a blank line to avoid the buffer becoming totally empty.
     */
    if (nnlines == 0 && firstp == buffer->b_line0 &&
					lastp == buffer->b_lastline) {
	/*
	 * We are going to delete all the lines - so we have
	 * to insert a new blank one in their place.
	 */
	{
	    unsigned savecho;

	    savecho = echo;
	    echo &= ~e_ALLOCFAIL;
	    new_start = newline(1);
	    echo = savecho;
	}
	if (new_start == NULL) {
	    show_error(window, "Can't get memory to delete lines");
	    chfree(change);
	    return(NULL);
	}
	new_end = new_start;
	nnlines = 1;
	nplines = 1;
    }

    /*
     * Scan through all of the windows which are mapped onto
     * the buffer being changed, and do any screen updates
     * that seem like a good idea.
     */
    wp = window;
    do {
	/*
	 * Only do windows onto the right buffer.
	 */
	if (wp->w_buffer != buffer)
	    continue;

	/*
	 * Redraw part of the screen if necessary.
	 */
	if (!earlier(line, wp->w_topline) &&
	    earlier(lastline, wp->w_botline)) {

	    int	start_row;

	    start_row = cntplines(wp, wp->w_topline, line);
	    if (nplines > oplines && start_row > 0 &&
		(start_row + nplines - oplines) < wp->w_nrows - 2) {

		s_ins(wp, start_row, (int) (nplines - oplines));

	    } else if (nplines < oplines &&
		    (start_row + oplines - nplines) < (wp->w_nrows - 2)) {

		s_del(wp, start_row, (int) (oplines - nplines));
	    }
	}
    } while ((wp = xvNextDisplayedWindow(wp)) != window);

    /*
     * Record the old set of lines as the replacement
     * set for undo, if there were any.
     */
    if (nolines > 0) {
	lastp->l_prev->l_next = NULL;
	line->l_prev = NULL;
	change->c_lines = line;
    } else {
	change->c_lines = NULL;
    }
    change->c_nlines = nnlines;

    /*
     * Link the buffer back together, using the new
     * lines if there are any, otherwise just linking
     * around the deleted lines.
     */
    if (new_start != NULL) {
	firstp->l_next = new_start;
	lastp->l_prev = new_end;
	new_end->l_next = lastp;
	new_start->l_prev = firstp;
    } else {
	firstp->l_next = lastp;
	lastp->l_prev = firstp;
    }

    buffer->b_flags |= FL_MODIFIED;

    /*
     * Re-link the buffer file pointer
     * in case we deleted line 1.
     */
    buffer->b_file = buffer->b_line0->l_next;

    /*
     * Update the w_cursor and w_topline fields in any Xviwins
     * for which the lines to which they were pointing have
     * been deleted.
     */
    wp = window;
    do {
	/*
	 * Only do windows onto the right buffer.
	 */
	if (wp->w_buffer != buffer)
	    continue;

	/*
	 * Don't need to update the w_cursor or w_topline
	 * elements of this window if no lines are being
	 * deleted.
	 */
	if (nolines == 0)
	    continue;

	/*
	 * Need a new cursor line value.
	 */
	if (wp->w_cursor->p_line == NULL) {
	    if (wp->w_cursor->p_index == 0) {
		wp->w_cursor->p_line = (lastp != buffer->b_lastline) ?
						lastp : lastp->l_prev;
	    } else {
		wp->w_cursor->p_line = firstp;
		(void) xvMoveDown(wp->w_cursor, (long) wp->w_cursor->p_index);
	    }
	    wp->w_cursor->p_index = 0;
	    begin_line(wp, TRUE);
	}

	/*
	 * Update the "topline" element of the Xviwin structure
	 * if the current topline is one of those being replaced.
	 */
	if (line->l_number <= wp->w_topline->l_number &&
			lastline->l_number >= wp->w_topline->l_number) {
	    wp->w_topline = wp->w_cursor->p_line;
	}
    } while ((wp = xvNextWindow(wp)) != window);

    /*
     * Renumber the buffer - but not until all the pointers,
     * especially the b_file pointer, have been re-linked.
     */
    {
	Line		*p;
	unsigned long	l;

	p = firstp;
	l = p->l_number;
	for (p = p->l_next; p != buffer->b_lastline; p = p->l_next) {
	    p->l_number = ++l;
	}
	buffer->b_lastline->l_number = MAX_LINENO;
    }

    cdp->cd_total_lines += nnlines - nolines;
    return(change);
}

/*
 * Replace the entire buffer with the specified list of lines.
 *
 * This is only used for the :edit command, and we assume that checking
 * has already been done that we are not losing data. The buffer is
 * marked as unmodified. No screen updating is performed.
 */
void
replbuffer(window, newlines)
register Xviwin	*window;
Line		*newlines;
{
    register Buffer	*buffer;	/* buffer window is mapped onto */
    Line		*new_end;	/* last line to be inserted */
    Line		*p;
    unsigned long	l;
    Xviwin		*wp;
    ChangeData		*cdp;
    Change		*chp;

    cdp = (ChangeData *) window->w_buffer->b_change;
    buffer = window->w_buffer;

    if (newlines == NULL) {
	show_error(window,
		"Internal error: replbuffer called with no lines");
	return;
    }

    if (cdp->cd_nlevels != 0) {
	show_error(window,
		"Internal error: replbuffer called with nlevels != 0");
	return;
    }

    /*
     * Remove all recorded changes for this buffer.
     */
    while (pop_change_list(cdp->cd_undo, &chp)) {
	free_changes(chp);
    }
    while (pop_change_list(cdp->cd_redo, &chp)) {
	free_changes(chp);
    }

    /*
     * Point new_end at the last line of newlines.
     */
    for (new_end = newlines; new_end->l_next != NULL;
    					new_end = new_end->l_next) {
	;
    }

    /*
     * Free up the old list of lines.
     */
    buffer->b_lastline->l_prev->l_next = NULL;
    throw(buffer->b_file);

    /*
     * Link the buffer back together with the new lines.
     */
    buffer->b_line0->l_next = buffer->b_file = newlines;
    newlines->l_prev = buffer->b_line0;
    buffer->b_lastline->l_prev = new_end;
    new_end->l_next = buffer->b_lastline;

    /*
     * Update the w_cursor and w_topline fields in all Xviwins
     * mapped onto the current Buffer.
     */
    wp = window;
    do {
	if (wp->w_buffer != buffer)
	    continue;

	move_cursor(wp, buffer->b_file, 0);
	wp->w_topline = wp->w_cursor->p_line;

    } while ((wp = xvNextWindow(wp)) != window);

    /*
     * Renumber the buffer.
     */
    l = 0L;
    for (p = buffer->b_line0; p != buffer->b_lastline; p = p->l_next) {
	p->l_number = l++;
    }
    buffer->b_lastline->l_number = MAX_LINENO;

    /*
     * Mark buffer as unmodified, and clear any marks it has.
     */
    buffer->b_flags &= ~FL_MODIFIED;
    init_marks(buffer);
}

/*
 * Perform an undo. Or a redo. Same thing. If specific is TRUE,
 * then we are moving in the direction specified (FORWARD or BACKWARD).
 * Otherwise, we are doing a vi-compatible undo, which means either a
 * redo or an undo depending on what we did last time. Sort of thing.
 */
void
undo(window, specific, direction)
Xviwin	*window;
bool_t	specific;
int	direction;
{
    register Buffer	*buffer;
    ChangeData		*cdp;
    Change		*chp;
    Change		*change;
    ChangeStack		*cstack;
    bool_t		can_undo;

    cdp = (ChangeData *) window->w_buffer->b_change;
    buffer = window->w_buffer;

    if (cdp->cd_nlevels != 0) {
	show_error(window, "Internal error: undo called with nlevels != 0");
	return;
    }

    /*
     * If we are a "normal" undo, then we first check the redo stack;
     * if it is non-empty, then we perform a redo, effectively undoing
     * the last undo. Otherwise we perform a real undo of the last cmd,
     * or a redo of the last (undone) command, according to direction.
     */
    if (specific) {
	can_undo = pop_change_list(
		(direction == FORWARD) ? cdp->cd_redo : cdp->cd_undo, &chp);
    } else {
	if (pop_change_list(cdp->cd_redo, &chp)) {
	    direction = FORWARD;
	    can_undo = TRUE;
	} else {
	    direction = BACKWARD;
	    can_undo = pop_change_list(cdp->cd_undo, &chp);
	}
    }
    if (!can_undo) {
	show_error(window, "Nothing to undo!");
	return;
    }

    /*
     * cstack points to the stack where we will be constructing the
     * opposite of the changes we are making, i.e. we push anti-changes
     * onto the redo stack if we are undoing and vice versa.
     */
    cstack = (direction == FORWARD) ? cdp->cd_undo : cdp->cd_redo;

    push_change_list(cstack);
    cdp->cd_total_lines = 0;
    cdp->cd_nlevels = 0;

    while (chp != NULL) {
	Change	*tmp;
	Line	*lp;

	tmp = chp;
	chp = chp->c_next;

	lp = gotoline(buffer, tmp->c_lineno);

	change = NULL;
	switch (tmp->c_type) {
	case C_LINE:
	    /*
	     * If the line corresponding to the given number
	     * isn't the one we went to, then we must have
	     * deleted it in the original change. If the change
	     * structure says to insert lines, we must set the
	     * line pointer to the lastline marker so that the
	     * insert happens in the right place; otherwise,
	     * we should not, because repllines won't handle
	     * deleting lines from the lastline pointer.
	     */
	    if (lp->l_number < tmp->c_lineno && tmp->c_lines != NULL) {
		lp = buffer->b_lastline;
	    }
	    /*
	     * Put the lines back as they were.
	     * Note that we don't free the lines
	     * here; that is only ever done by
	     * free_changes, when the next line
	     * change happens.
	     */
	    change = _repllines(window, lp, tmp->c_nlines, tmp->c_lines);
	    break;

	case C_DEL_CHAR:
	    change = _replchars(window, lp, tmp->c_index, tmp->c_nchars, "");
	    break;

	case C_CHAR:
	    change = _replchars(window, lp, tmp->c_index, tmp->c_nchars,
							    tmp->c_chars);

	    /*
	     * Free up the string, since it was strsave'd
	     * by replchars at the time the change was made.
	     */
	    free(tmp->c_chars);
	    break;

	case C_POSITION:
	    move_cursor(window,
	    		gotoline(buffer, (unsigned long) tmp->c_pline),
			tmp->c_pindex);
	    break;

	default:
	    show_error(window,
	     "Internal error in undo: invalid change type. This is serious.");
	    break;
	}
	if (change != NULL) {
	    push_change(cstack, change);
	}
	chfree(tmp);
    }

    xvUpdateAllBufferWindows(buffer);
}

static void
free_changes(chp)
Change	*chp;
{
    while (chp != NULL) {
	Change	*tmp;

	tmp = chp;
	chp = chp->c_next;

	switch (tmp->c_type) {
	case C_LINE:
	    throw(tmp->c_lines);
	    break;
	case C_CHAR:
	    free(tmp->c_chars);
	    break;
	case C_DEL_CHAR:
	case C_POSITION:
	    break;
	}
	chfree(tmp);
    }
}

static void
report(window)
Xviwin	*window;
{
    register ChangeData	*cdp;

    cdp = (ChangeData *) window->w_buffer->b_change;

    if (echo & e_REPORT) {
	if (cdp->cd_total_lines > Pn(P_report)) {
	    show_message(window, "%ld more lines", cdp->cd_total_lines);
	} else if (-(cdp->cd_total_lines) > Pn(P_report)) {
	    show_message(window, "%ld fewer lines", -(cdp->cd_total_lines));
	}
    }
}

bool_t
set_edit(window, new_value, interactive)
Xviwin		*window;
Paramval	new_value;
bool_t		interactive;
{
    Xviwin	*wp;

    /*
     * Disallow setting of "edit" parameter to TRUE if it is FALSE.
     * Hence, this parameter can only ever be set to FALSE.
     */
    if (new_value.pv_b == TRUE && !Pb(P_edit)) {
	if (interactive) {
	    show_error(window, "Can't set edit once it has been unset");
	}
	return(FALSE);
    } else {
	/*
	 * Set the "noedit" flag on all current buffers,
	 * but only if we are in interactive mode
	 * (otherwise the window pointer is unreliable).
	 * This may set the flag several times on split
	 * buffers, but it's no great problem so why not.
	 */
	if (interactive) {
	    wp = window;
	    do {
		wp->w_buffer->b_flags |= FL_NOEDIT;
	    } while ((wp = xvNextWindow(wp)) != window);
	}
	return(TRUE);
    }
}

bool_t
set_undolevels(window, new_value, interactive)
Xviwin		*window;
Paramval	new_value;
bool_t		interactive;
{
    ChangeStack	*csp = ((ChangeData *) window->w_buffer->b_change)->cd_undo;
    int i;

    /*
     * Limits on undolevels: 1 to MAX_UNDO.
     */
    if ((i = new_value.pv_i) < 1 || i > MAX_UNDO) {
	if (interactive) {
	    show_error(window,
		"The value for undolevels must be between 1 and %d", MAX_UNDO);
	}
	return(FALSE);
    }

    /*
     * Discard any changes if the limit has been reduced.
     */
    while (csp->cs_size > i) {
	free_changes(csp->cs_stack[csp->cs_size - 1]);
	csp->cs_size -= 1;
    }

    return(TRUE);
}
