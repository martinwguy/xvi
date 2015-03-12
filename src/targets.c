/* Copyright (c) 1990,1991,1992,1993,1994 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    targets.c
* module function:
    Routines for finding "targets", which may be used either to move
    the cursor or with an operator to affect the contents of a buffer.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"
#include "cmd.h"

/*
 * Handle cursor movement commands.
 * These are used simply to move the cursor somewhere,
 * and also as targets for the various operators.
 *
 * If the value of cmd_target.p_line upon return is NULL, the caller
 * will complain loudly to the user and cancel any outstanding operator.
 * The cursor should hopefully not have moved.
 *
 * Arguments are the first and second characters (where appropriate).
 *
 * Third argument is the prefix count, or 1 if none was given.
 */
void
do_target(cmd)
Cmd	*cmd;
{
    bool_t	skip_spaces;
    Posn	lastpos;
    Posn	*pp;

    skip_spaces = FALSE;
    lastpos = *(curwin->w_cursor);

    switch (cmd->cmd_ch1) {
    case 'G':
	cmd->cmd_target.p_line = gotoline(curbuf,
				(unsigned long) ((cmd->cmd_prenum > 0) ?
					cmd->cmd_prenum : MAX_LINENO));
	cmd->cmd_target.p_index = 0;
	xvSetPosnToStartOfLine(&(cmd->cmd_target), TRUE);
	skip_spaces = TRUE;
	break;

    case '-':
	skip_spaces = TRUE;
	/* FALL THROUGH */
    case 'k':
    case K_UARROW:
    case CTRL('P'):
	if (xvMoveUp(&lastpos, LDEF1(cmd->cmd_prenum))) {
	    xvMoveToColumn(&lastpos, curwin->w_curswant);
	    cmd->cmd_target = lastpos;
	}
	break;

    case '+':
    case '\r':
	skip_spaces = TRUE;
	/* FALL THROUGH */
    case '\n':
    case 'j':
    case K_DARROW:
    case CTRL('N'):
	if (xvMoveDown(&lastpos, LDEF1(cmd->cmd_prenum))) {
	    xvMoveToColumn(&lastpos, curwin->w_curswant);
	    cmd->cmd_target = lastpos;
	}
	break;

    /*
     * This is a strange motion command that helps make
     * operators more logical. It is actually implemented,
     * but not documented in the real 'vi'. This motion
     * command actually refers to "the current line".
     * Commands like "dd" and "yy" are really an alternate
     * form of "d_" and "y_". It does accept a count, so
     * "d3_" works to delete 3 lines.
     */
    case '_':
	(void) xvMoveDown(&lastpos, LDEF1(cmd->cmd_prenum) - 1);
	cmd->cmd_target = lastpos;
	break;

    case '|':
	xvMoveToColumn(&lastpos, LONG2INT(cmd->cmd_prenum - 1));
	curwin->w_curswant = cmd->cmd_prenum - 1;
	cmd->cmd_target = lastpos;
	break;

    case '%':
	pp = showmatch();
	if (pp != NULL) {
	    cmd->cmd_target = *pp;
	}
	break;

    case '$':
	(void) xvMoveDown(&lastpos, LDEF1(cmd->cmd_prenum) - 1);
	while (xvMoveRight(&lastpos, FALSE))
	    ;
	cmd->cmd_target = lastpos;
	curwin->w_curswant = INT_MAX;
	curwin->w_set_want_col = FALSE;
	break;

    case '^':
    case '0':
	xvSetPosnToStartOfLine(&lastpos, cmd->cmd_ch1 == '^');
	cmd->cmd_target = lastpos;
	break;

    case 'n':
    case 'N':
	pp = xvDoSearch(curwin, "", cmd->cmd_ch1);
	if (pp != NULL) {
	    cmd->cmd_target = *pp;
	}
	break;

    case '(':
    case ')':
    case '{':
    case '}':
    case '[':
    case ']':
    {
	int	num;

	for (num = IDEF1(cmd->cmd_prenum); num > 0; --num) {
	    pp = xvLocateTextObject(curwin, &lastpos, cmd->cmd_ch1, cmd->cmd_ch2);
	    if (pp != NULL) {
		lastpos = *pp;
	    } else {
		break;
	    }
	}

	if (lastpos.p_line != curwin->w_cursor->p_line ||
			    lastpos.p_index != curwin->w_cursor->p_index) {
	    cmd->cmd_target = lastpos;
	}
	break;
    }

    case '\'':
    case '`':
	pp = getmark(cmd->cmd_ch2, curbuf);
	if (pp != NULL) {
	    if (cmd->cmd_ch1 == '\'') {
		skip_spaces = TRUE;
		pp->p_index = 0;
	    }
	    cmd->cmd_target = *pp;
	}
	break;

    case '/':
    case '?':
	/*
	 * We don't return a position here as these targets are deferred.
	 */
	cmd_init(curwin, cmd->cmd_ch1);
    }

    if (cmd->cmd_target.p_line != NULL && skip_spaces) {
	xvSetPosnToStartOfLine(&(cmd->cmd_target), TRUE);
    }
}

void
do_left_right(cmd)
Cmd	*cmd;
{
    Posn		pos;
    register bool_t	(*mvfunc) P((Posn *, bool_t));
    register long	n;
    register long	i;

    pos = *(curwin->w_cursor);
    mvfunc = (cmd->cmd_ch1 == 'l' || cmd->cmd_ch1 == ' ' || cmd->cmd_ch1 == K_RARROW) ?
    					xvMoveRight : xvMoveLeft;
    n = LDEF1(cmd->cmd_prenum);
    for (i = 0; i < n; i++) {
	if (!(*mvfunc)(&pos, FALSE)) {
	    break;
	}
    }
    if (i != 0) {
	cmd->cmd_target = pos;
    }
}

/*
 * Handle word motion ('w', 'W', 'b', 'B', 'e' or 'E').
 */
void
do_word(cmd)
Cmd	*cmd;
{
    register Posn	*(*func) P((Posn *, int, bool_t));
    register long	n;
    register int	lc;
    register int	type;
    Posn		pos;

    if (is_upper(cmd->cmd_ch1)) {
	type = 1;
	lc = to_lower(cmd->cmd_ch1);
    } else {
	type = 0;
	lc = cmd->cmd_ch1;
    }

    switch (lc) {
    case 'b':
	func = bck_word;
	break;

    case 'w':
	func = fwd_word;
	break;

    case 'e':
	func = end_word;
	break;
    }

    pos = *curwin->w_cursor;

    for (n = LDEF1(cmd->cmd_prenum); n > 0; n--) {
	Posn	*newpos;
	bool_t	skip_whites;

	/*
	 * "cw" is a special case; the whitespace after
	 * the end of the last word involved in the change
	 * does not get changed. The following code copes
	 * with this strangeness.
	 */
	if (n == 1 && cmd->cmd_operator == 'c' && lc == 'w') {
	    skip_whites = FALSE;
	    MakeInclusive(cmd);
	} else {
	    skip_whites = TRUE;
	}

	newpos = (*func)(&pos, type, skip_whites);

	if (newpos == NULL) {
	    return;
	}

	if (n == 1 && lc == 'w' && cmd->cmd_operator != NOP &&
					newpos->p_line != pos.p_line) {
	    /*
	     * We are on the last word to be operated
	     * upon, and have crossed the line boundary.
	     * This should not happen, so back up to
	     * the end of the line the word is on.
	     */
	    while (dec(newpos) == mv_SAMELINE)
		;
	    MakeInclusive(cmd);
	}

	if (skip_whites == FALSE) {
	    (void) dec(newpos);
	}
	pos = *newpos;
    }
    cmd->cmd_target = pos;
}

void
do_csearch(cmd)
Cmd	*cmd;
{
    Posn	*pos;
    int		dir;

    switch (cmd->cmd_ch1) {
    case 'T':
    case 't':
    case 'F':
    case 'f':
	if (is_upper(cmd->cmd_ch1)) {
	    dir = BACKWARD;
	    cmd->cmd_ch1 = to_lower(cmd->cmd_ch1);
	} else {
	    dir = FORWARD;
	}

	pos = searchc(cmd->cmd_ch2, dir, (cmd->cmd_ch1 == 't'), IDEF1(cmd->cmd_prenum));
	break;

    case ',':
    case ';':
	/*
	 * This should be FALSE for a backward motion.
	 * How do we know it's a backward motion?
	 *
	 * Fix it later.
	 */
	MakeInclusive(cmd);
	pos = crepsearch(curbuf, cmd->cmd_ch1 == ',', IDEF1(cmd->cmd_prenum));
	break;
    }
    if (pos != NULL) {
	cmd->cmd_target = *pos;
    }
}

/*
 * Handle home ('H') end of page ('L') and middle line ('M') motion commands.
 */
void
do_HLM(cmd)
Cmd	*cmd;
{
    register Line	*dest;
    register Line	*top;
    register Line	*bottom;
    Posn		pos;

    if (cmd->cmd_ch1 == K_HOME) {
	cmd->cmd_ch1 = 'H';
    }

    /*
     * Silly to specify a number before 'H' or 'L'
     * which would move us off the screen.
     */
    if (cmd->cmd_prenum >= curwin->w_nrows) {
	return;
    }

    dest = NULL;
    top = curwin->w_topline;
    bottom = curwin->w_botline->l_prev;
    switch (cmd->cmd_ch1)
    {
	case 'H':
	{
	    register long n;

	    dest = top;
	    for (n = cmd->cmd_prenum - 1; n > 0 && dest != bottom; --n)
		dest = dest->l_next;
	    break;
	}
	case 'L':
	{
	    register long n;

	    dest = bottom;
	    for (n = cmd->cmd_prenum - 1; n > 0 && dest != top; --n)
		dest = dest->l_prev;
	    break;
	}
	case 'M':
	{
	    register unsigned long middle;

	    middle = (top->l_number + bottom->l_number) / 2;
	    for (dest = top; dest->l_number < middle
			     && dest != bottom; dest = dest->l_next)
		;
	}
    }
    if (dest) {
	pos.p_line = dest;
	pos.p_index = 0;
	xvSetPosnToStartOfLine(&pos, TRUE);
	cmd->cmd_target = pos;
    }
}
