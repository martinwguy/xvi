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
#include "cmd.h"	/* for TEXT_INPUT */

static	bool_t	save_position P((Change **));
static	bool_t	init_change_data P((void));
static	void	free_changes P((Change *));
static	Change	*_replchars P((Line *, int, int, char *));
static	Change	*_repllines P((Line *, long, Line *));
static	void	report P((void));

void
init_undo(buffer)
Buffer	*buffer;
{
    ChangeData	*cdp;

    /*
     * Initialise the undo-related variables in the Buffer.
     */
    cdp = alloc(sizeof(ChangeData));
    if (cdp == NULL) {
	return;
    }
    cdp->cd_nlevels = 0;
    cdp->cd_undo = NULL;

    buffer->b_change = cdp;

    buffer->b_prevline = NULL;
    buffer->b_Undotext = NULL;

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

    cdp = buffer->b_change;

    /*
     * Remove all recorded changes for this buffer.
     */
    free_changes(cdp->cd_undo);
    cdp->cd_undo = NULL;

    free(buffer->b_change);
}

static void
push_change(csp, change)
Change		**csp;
Change		*change;
{
    change->c_next = *csp;
    *csp = change;
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
start_command(cmd)
Cmd *	cmd;		/* cmd for vi commands, NULL for ex commands */
{
    ChangeData	*cdp = curbuf->b_change;

    if (!init_change_data()) {
	return(FALSE);
    }

    if (cdp->cd_nlevels == 0) {
	cdp->cd_vi_cmd = cmd;
    }

    cdp->cd_nlevels += 1;

    return(TRUE);
}

static bool_t
init_change_data()
{
    ChangeData	*cdp = curbuf->b_change;

    if (cdp->cd_nlevels == 0) {
	free_changes(cdp->cd_undo);
	cdp->cd_undo = NULL;
	cdp->cd_total_lines = 0;
	cdp->cd_nlevels = 0;
	cdp->cd_vi_cmd = NULL;
	if (!save_position(&(cdp->cd_undo))) {
	    return(FALSE);
	}
    }
    return(TRUE);
}

/*
 * Save the cursor position.
 *
 * This is called at the start of each change, so that
 * the cursor will return to the right place after an undo.
 */
static bool_t
save_position(csp)
Change **csp;		/* Stack where to save the change */
{
    register Change	*change;

    change = challoc();
    if (change == NULL) {
	return(FALSE);
    }

    change->c_type = C_POSITION;
    change->c_lineno =
    change->c_pline = lineno(curwin->w_cursor->p_line);
    change->c_pindex = curwin->w_cursor->p_index;

    push_change(csp, change);

    return(TRUE);
}

void
end_command()
{
    register ChangeData	*cdp = curbuf->b_change;

    if (cdp->cd_nlevels > 0) {
	cdp->cd_nlevels -= 1;
	if (cdp->cd_nlevels == 0) {
	    report();
	}
    } else {
	show_error("Internal error: too many \"end_command\"s");
    }
}

/*
 * Interface used by rest of editor code to _replchars(). We do two
 * extra things here: call init_change_data() and push the anti-change
 * onto the undo stack. These are only valid when we are making a real
 * change; _replchars() gets called when we are undoing a change,
 * when we don't want those things to happen.
 */
void
replchars(line, start, nchars, newstring)
Line	*line;
int	start;
int	nchars;
char	*newstring;
{
    ChangeData	*cdp = curbuf->b_change;
    Change	*change;

    if (!init_change_data()) {
	return;
    }

    change = _replchars(line, start, nchars, newstring);

    /*
     * Push this change onto the LIFO of changes
     * that form the current command.
     */
    if (change != NULL) {
	push_change(&(cdp->cd_undo), change);
    }
}

/*
 * Interface used by rest of editor code to _repllines(). We do two
 * extra things here: call init_change_data() and push the anti-change
 * onto the undo stack. These are only valid when we are making a real
 * change; _repllines() gets called when we are undoing a change,
 * when we don't want those things to happen.
 */
void
repllines(line, nolines, newlines)
Line		*line;
long		nolines;
Line		*newlines;
{
    ChangeData	*cdp = curbuf->b_change;
    Change	*change;

    if (!init_change_data()) {
	return;
    }

    change = _repllines(line, nolines, newlines);

    /*
     * Push this change onto the LIFO of changes
     * that form the current command.
     */
    if (change != NULL) {
	push_change(&(cdp->cd_undo), change);
    }

    if (cdp->cd_nlevels == 0) {
	report();
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
_replchars(line, start, nchars, newstring)
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
    Buffer		*buffer = curbuf;
    Change		*change;

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
    change->c_lineno = lineno(line);
    change->c_index = start;
    change->c_nchars = nlen;

    if (offset > 0) {
	register char	*s;

	/*
	 * Move existing text along by offset to the right.
	 * First make some room in the line.
	 */
	if (grow_line(line, offset) == FALSE) {
	    if (nchars != 0) free(change->c_chars);
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
_repllines(line, nolines, newlines)
Line		*line;
long		nolines;
Line		*newlines;
{
    Xviwin		*window = curwin;
    Xviwin		*savecurwin;
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

    buffer = curbuf;
    cdp = buffer->b_change;

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
	    nplines += plines(new_end);
	    restoremarks(new_end, buffer);
	}
	nplines += plines(new_end);
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

	oplines += plines(lastp);

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
		if (sizeof(long) > sizeof(int) && offset > INT_MAX) {
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
	    new_start->l_number = 0;
	    echo = savecho;
	}
	if (new_start == NULL) {
	    show_error(out_of_memory);
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
    savecurwin = curwin;
    do {
	/*
	 * Only do windows onto the right buffer.
	 */
	if (curbuf != buffer)
	    goto cont1;

	/*
	 * Redraw part of the screen if necessary.
	 */
	if (!earlier(line, curwin->w_topline) &&
	    earlier(lastline, curwin->w_botline)) {

	    int	start_row;

	    start_row = cntplines(curwin->w_topline, line);
	    if (nplines > oplines && start_row > 0 &&
		(start_row + nplines - oplines) < curwin->w_nrows - 2) {

		s_ins(start_row, (int) (nplines - oplines));

	    } else if (nplines < oplines &&
		    (start_row + oplines - nplines) < (curwin->w_nrows - 2)) {

		s_del(start_row, (int) (oplines - nplines));
	    }
	}
cont1:	set_curwin(xvNextDisplayedWindow(curwin));
    } while (curwin != savecurwin);

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
    do {
	wp = curwin;

	/*
	 * Only do windows onto the right buffer.
	 */
	if (wp->w_buffer != buffer)
	    goto cont2;

	/*
	 * Don't need to update the w_cursor or w_topline
	 * elements of this window if no lines are being
	 * deleted.
	 */
	if (nolines == 0)
	    goto cont2;

	/*
	 * Need a new cursor line value.
	 */
	if (wp->w_cursor->p_line == NULL) {
	    if (wp->w_cursor->p_index == 0) {
		wp->w_cursor->p_line = (lastp != buffer->b_lastline) ?
						lastp : lastp->l_prev;
	    } else {
		wp->w_cursor->p_line = firstp;
		(void) xvMoveDown(wp->w_cursor, (long) wp->w_cursor->p_index,
				  FALSE);	/* Can't fail */
	    }
	    wp->w_cursor->p_index = 0;
	    begin_line(TRUE);
	}

	/*
	 * Update the "topline" element of the Xviwin structure
	 * if the current topline is one of those being replaced.
	 */
	if (line->l_number <= wp->w_topline->l_number &&
			lastline->l_number >= wp->w_topline->l_number) {
	    wp->w_topline = wp->w_cursor->p_line;
	}
cont2:	set_curwin(xvNextWindow(wp));
    } while (curwin != savecurwin);

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
replbuffer(newlines)
Line		*newlines;
{
    Xviwin		*savecurwin;
    register Buffer	*buffer = curbuf; /* buffer window is mapped onto */
    ChangeData		*cdp = curbuf->b_change;
    Line		*new_end;	/* last line to be inserted */
    Line		*p;
    unsigned long	l;

    if (newlines == NULL) {
	show_error("Internal error: replbuffer called with no lines");
	return;
    }

    if (cdp->cd_nlevels != 0) {
	show_error("Internal error: replbuffer called with nlevels != 0");
	return;
    }

    /*
     * Remove all recorded changes for this buffer.
     */
    free_changes(cdp->cd_undo);
    cdp->cd_undo = NULL;

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
    savecurwin = curwin;
    do {
	if (curbuf == buffer) {
	    move_cursor(buffer->b_file, 0);
	    curwin->w_topline = curwin->w_cursor->p_line;
	}
        set_curwin(xvNextWindow(curwin));
    } while (curwin != savecurwin);

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
 * Undo all changes made to the line since we moved onto it by restoring
 * it from the copy we made of its text when we moved onto it.
 *
 * POSIX: "Restore the current line to its state immediately before
 * the most recent time that it became the current line.
 * Current column: Set to the first column in the line in which any portion
 * of the first character in the line is displayed."
 *
 * They don't say how a normal 'u'ndo should behave after 'U' line undo, but
 * classic vi undoes the line-Undo command and if you repeatedly line-Undo
 * and then 'u'ndo, you still get the messed up line back.
 */
void
undoline()
{
    Line   *line = curwin->w_cursor->p_line;
    Buffer *buffer = curbuf;

    /*
     * Set the undo history to a command that restores the line to
     * the way it was before they did the line undo, but only if the line
     * has changed (so that UUu still messes it up again).
     */
    if (strcmp(line->l_text, buffer->b_Undotext) != 0) {
	buffer->b_change->cd_nlevels = 0;
	init_change_data();
	replchars(line, 0, strlen(line->l_text), buffer->b_Undotext);
    }
    move_cursor(line, 0);

    xvUpdateAllBufferWindows();
}

/*
 * Perform an undo.
 */
void
undo()
{
    register Buffer	*buffer;
    ChangeData		*cdp;
    Change		*chp;
    Change		*change;
    Change		*redo;
    /*
     * Variables to know Which line to leave the cursor on
     */
    int			firstlinechanged = INT_MAX;
    int			firstlinedeleted = INT_MAX;
    /* Type of last change; C_POSITION is a no-action value */
    enum c_type_t	last_change_type = C_POSITION;
    /* The start of the last inserted text or where a deletion started */
    int			last_index = 0;
    /*
     * To find out if a single line was modified, we set this to -1 to mean
     * nothing modified yet, a line number to say which single line has been
     * modified or -2 if more than one line has been modified.
     */
    int			lines_modified = -1;	/* None */

    cdp = curbuf->b_change;
    buffer = curbuf;

    if (cdp->cd_nlevels != 0) {
	show_error("Internal error: undo called with nlevels != 0");
	return;
    }

    /*
     * chp points to the list of changes to make to undo the last change.
     */
    chp = cdp->cd_undo;
    if (chp == NULL) {
	show_error("Nothing to undo!");
	return;
    }

    /*
     * "redo" is the stack where we will be constructing the opposite
     * of the changes we are making, i.e. we push anti-changes
     * onto the redo stack.  First, record the cursor position.
     */
    redo = NULL;
    if (!save_position(&redo)) {
	return;
    }

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
	    change = _repllines(lp, tmp->c_nlines, tmp->c_lines);
	    if (tmp->c_lines == NULL) {
		/*
		 * If no lines were inserted; this was a line deletion.
		 * Deleting the last line(s) of the file which leaves fld>EOF
		 * is handled when fld is used.
		 */
		if (lp->l_number < firstlinedeleted) {
		    firstlinedeleted = lp->l_number;
		}
	    } else {
		/*
		 * This was a line change or insertion;
		 * remember the first non-blank of the first inserted line
		 */
		if (lp->l_number < firstlinechanged) {
		    Posn pos;

		    firstlinechanged = lp->l_number;
		    pos.p_line = lp;
		    xvSetPosnToStartOfLine(&pos, TRUE);
		    last_index = pos.p_index;
		}
	    }

	    break;

	case C_DEL_CHAR:
	    change = _replchars(lp, tmp->c_index, tmp->c_nchars, "");
	    /*
	     * POSIX: "If text was deleted, set to [...]
	     *         the first character after the deleted text
	     */
	    last_index = tmp->c_index;
	    if (last_index > 0 && lp->l_text[last_index] == '\0') {
		last_index--;
	    }
	    last_change_type = tmp->c_type;

	    /* Deleting characters counts as changing a line */
	    if (lp->l_number < firstlinechanged) {
		firstlinechanged = lp->l_number;
	    }

	    switch (lines_modified) {
	    case -1:
		lines_modified = lp->l_number;
		break;
	    case -2:
		/* Still many lines modified */
		break;
	    default:
		if (lines_modified != lp->l_number) {
		    lines_modified = -2;
		} else {
		    /* Modifying the same line */
		}
		break;
	    }
	    break;

	case C_CHAR:
	    change = _replchars(lp, tmp->c_index, tmp->c_nchars, tmp->c_chars);
	    /*
	     * Free up the string, since it was strsave'd
	     * by replchars at the time the change was made.
	     */
	    free(tmp->c_chars);

	    if (lp->l_number < firstlinechanged) {
		firstlinechanged = lp->l_number;
	    }
	    last_change_type = tmp->c_type;
	    /*
	     * POSIX: "If text was added or changed, set to [...]
	     *         the first character added or changed"
	     */
	    last_index = tmp->c_index;
	    break;

	case C_POSITION:
	    move_cursor(gotoline(buffer, (unsigned long) tmp->c_pline),
			tmp->c_pindex);
	    break;

	default:
	    show_error("Internal error in undo: invalid change type.");
	    break;
	}
	if (change != NULL) {
	    push_change(&redo, change);
	}

	chfree(tmp);
    }

    /*
     * POSIX:
     *
     * "Current line: Set to the first line added or changed if any;
     *  otherwise, move to the line preceding any deleted text if one exists;
     *  otherwise, move to line 1.
     *
     *  Current column: If undoing an ex command, set to the first non-<blank>.
     *  Otherwise, if undoing a text input command:
     *
     *  1. If the command was a C, c, O, o, R, S, or s command, the
     *     current column shall be set to the value it held when the text
     *     input command was entered.
     *
     *  2. Otherwise, set to the last column in which any portion of the
     *     first character after the deleted text is displayed,
     *     or, if no non-<newline> characters follow the text deleted
     *     from this line, set to the last column in which any portion
     *     of the last non-<newline> in the line is displayed,
     *     or 1 if the line is empty.
     *
     *	Otherwise, if a single line was modified (that is, not added
     *	or deleted) by the u command:
     *
     *	1. If text was added or changed, set to the last column in
     *	   which any portion of the first character added or changed
     *	   is displayed.
     *
     *	2. If text was deleted, set to the last column in which any
     *	   portion of the first character after the deleted text is
     *	   displayed, or, if no non-<newline> characters follow the
     *	   deleted text, set to the last column in which any portion
     *	   of the last non- <newline> in the line is displayed, or 1 if
     *	   the line is empty.
     *
     *	Otherwise, set to non-<blank>."
     */
    {
	int	endpos;		/* Line number of the line to end up on */
	Posn	p;

	if (firstlinechanged != INT_MAX) {
	    endpos = firstlinechanged;
	} else if (firstlinedeleted != INT_MAX) {
	    endpos = firstlinedeleted - 1;
	} else {
	    endpos = bufempty() ? 0 : 1;
	}

	p.p_line = gotoline(buffer, (unsigned long) endpos);

	/* Set cursor position within line according to above rules */
	if (cdp->cd_vi_cmd == NULL) {
            xvSetPosnToStartOfLine(&p, TRUE);
	} else {
	    Cmd *	cmd = cdp->cd_vi_cmd;

	    /* vi mode command */
	    if (CFLAGS(cmd->cmd_ch1) & TEXT_INPUT) {
		if (strchr("CcOoRSs", cmd->cmd_ch1) != NULL) {
		    /* Keep the value set by undo's POSITION entry */
		    ;
		} else {
		    p.p_index = last_index;
		}
	    } else {
		/*
		 * if a single line was modified
		 * - if it added or changed test, the last char of that text
		 * - if it deleted text, to first char after deletion or EOL
		 */
		p.p_index = last_index;
	    }
	}

	move_cursor(p.p_line, p.p_index);
    }

    cdp->cd_undo = redo;

    xvUpdateAllBufferWindows();
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
report()
{
    register ChangeData	*cdp;

    cdp = curbuf->b_change;

    if (echo & e_REPORT) {
	if (cdp->cd_total_lines > Pn(P_report)) {
	    show_message("%ld more lines", cdp->cd_total_lines);
	} else if (-(cdp->cd_total_lines) > Pn(P_report)) {
	    show_message("%ld fewer lines", -(cdp->cd_total_lines));
	}
    }
}
