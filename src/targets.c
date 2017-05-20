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
	if (cmd->cmd_prenum > lineno(b_last_line_of(curbuf))) {
	    cmd->cmd_target.p_line = NULL;	/* Make the command fail */
	    return;
	}
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
	if (xvMoveUp(&lastpos, LDEF1(cmd->cmd_prenum), FALSE)) {
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
	if (xvMoveDown(&lastpos, LDEF1(cmd->cmd_prenum), FALSE)) {
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
	if (xvMoveDown(&lastpos, LDEF1(cmd->cmd_prenum) - 1, FALSE)) {
	    cmd->cmd_target = lastpos;
	}
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
    case K_END:
	/*
	 * POSIX: "If count is 1, it shall be an error if the line is empty."
	 * Classic vi, nvi and vim do not error for $ on an empty line, and
	 * it breaks the maze macros, so we only fail it if POSIXLY_CORRECT
	 */
	if (LDEF1(cmd->cmd_prenum) == 1) {
	    if (Pb(P_posix) && lastpos.p_line->l_text[0] == '\0') {
		/* Leave cmd->cmd_target NULL to flag an error */
		break;
	    }
	    /* MakeCharBased(cmd); */	/* a no-op, as d, c, y are char-based */
	} else {
	    /*
	     * POSIX: "It shall be an error if there are less than (count -1)
	     *         lines after the current line in the edit buffer."
	     */
	    if (!xvMoveDown(&lastpos, LDEF1(cmd->cmd_prenum) - 1, FALSE)) {
		break;
	    }
	}

	while (xvMoveRight(&lastpos, FALSE))
		;
	cmd->cmd_target = lastpos;
	curwin->w_curswant = INT_MAX;
	curwin->w_set_want_col = FALSE;

	/*
	 * POSIX: "[if count > 1 and] the starting cursor position is at
	 * or before the first non-<blank> in the line, the text region
	 * shall consist of the current and the next count -1 lines, and
	 * any text saved to a buffer shall be in line mode."
	 */
	if (LDEF1(cmd->cmd_prenum) > 1) {
	    Posn firstnonwhite;

	    firstnonwhite = *(curwin->w_cursor);
	    xvSetPosnToStartOfLine(&firstnonwhite, TRUE);
	    if (cmd->cmd_startpos.p_index <= firstnonwhite.p_index) {
		MakeLineBased(cmd);
	    }
	}
	break;

    case '^':
    case '0':
    case K_HOME:  /* For kH, nvi and elvis do ^ and vim does 0. We do ^. */
	xvSetPosnToStartOfLine(&lastpos, cmd->cmd_ch1 != '0');
	cmd->cmd_target = lastpos;
	break;

    case 'n':
    case 'N':
	pp = xvDoSearch("", cmd->cmd_ch1);
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
	    pp = xvLocateTextObject(&lastpos, cmd->cmd_ch1, cmd->cmd_ch2);
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
      {
	Posn		*posn;

	posn = getmark(cmd->cmd_ch2, curbuf);
	if (posn == NULL) {
	    show_error("Unknown mark");
	} else {
	    cmd->cmd_target.p_line = posn->p_line;
	    if (cmd->cmd_ch1 == '\'') {
	        /* cmd->cmd_target.p_index is set by xvSetPosnToStartOfLine() */
		skip_spaces = TRUE;
	    } else {
		cmd->cmd_target.p_index = posn->p_index;
		curwin->w_set_want_col = TRUE;
	    }
	}
	break;
      }
    case '/':
    case '?':
	/*
	 * We don't return a position here as these targets are deferred.
	 */
	cmd_init(cmd->cmd_ch1);
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

	/*
	 * Special case for deleting the last word of the file:
	 * fwd_word() returns the last char of the word
	 * instead of the following space char, so the
	 * command now needs to include the target character.
	 * If, instead, the file ends with "word)", fwd_word() leaves us
	 * sitting on the ), but that should NOT be made inclusive.
	 */
	if ( n == 1 && cmd->cmd_operator == 'd' && lc == 'w' &&
	     newpos->p_line == b_last_line_of(curbuf) &&
	     newpos->p_line->l_text[newpos->p_index + 1] == '\0' &&
	     ( is_alnum(gchar(newpos)) || gchar(newpos) == '_' ||
	       is_space(gchar(newpos)) ) ) {
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
    } else {
	/* When redoing, a failed search's replacement text gets executed as
	 * vi commands, so if a search fails, cancel stuffed input
	 */
	unstuff();
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
