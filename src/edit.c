/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    edit.c
* module function:
    Insert and replace mode handling.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

/*
 *	Global variables
 */

/*
 * literal_next is TRUE when they've hit ^V but not hit the actual char yet.
 * It is used in map.c to prevent literal chars from being put through "map!".
 */
bool_t	literal_next = FALSE;

/*
 *	Local function prototypes
 */

static	void	end_replace P((int));

/*
 *	Local variables
 */

/*
 * Position of start of insert. This is used
 * to prevent backing up past the starting point.
 */
static	Posn	Insertloc;

/*
 * This flexbuf is used to hold the current insertion text.
 */
static	Flexbuf	Insbuff;

/*
 * This is the number of times to repeat an insertion.
 */
static	int	Ins_repeat;

/*
 * Replace-mode stuff.
 */
static	enum	{
		replace_one,	/* replace command was an 'r' */
		got_one,	/* normal ending state for replace_one */
		overwrite	/* replace command was an 'R' */
}		repstate;

static	char	*saved_line;	/*
				 * record of old line before replace
				 * started; note that, if
				 * (repstate != overwrite), this
				 * should never be referenced.
				 */
static	int	nchars;		/* no of chars in saved_line */
static	int	start_index;	/* index into line where we entered replace */
static	int	start_column;	/* virtual col corresponding to start_index */

/*
 * Process the given character, in insert mode and in replace mode.
 *
 * Return TRUE if the screen needs repainting, FALSE if it doesn't.
 */
static bool_t ir_proc P((int, bool_t));

bool_t
i_proc(c)
int	c;
{
	return(ir_proc(c, TRUE));
}

bool_t
r_proc(c)
int	c;
{
	return(ir_proc(c, FALSE));
}

static bool_t
ir_proc(c, insert)
int	c;
bool_t	insert;
{
    register Posn	*curpos;
    static bool_t	wait_buffer = FALSE;

    /* These are used in insert mode only */
    bool_t		beginline;
    int			nlines;

    curpos = curwin->w_cursor;

    /*
     * Get the number of physical lines the current line occupies.
     */
    if (insert) {
	nlines = plines(curpos->p_line);
    }

    if (kbdintr) {
	kbdintr = FALSE;
	if (literal_next) {
	    c = kbdintr_ch;
	} else {
	    imessage = TRUE;
	    /*
	     * POSIX: "A keyboard interrupt in insert or replace mode should
	     * behave identically to pressing ESC".
	     */
	    wait_buffer = FALSE;
	    goto case_kbdintr_ch;
	}
    }

    if ((insert || repstate == overwrite) &&
	(wait_buffer || (!literal_next && c == CTRL('A')))) {
	/*
	 * Add contents of named buffer, or the last
	 * insert buffer if CTRL('A') was typed.
	 */
	if (!wait_buffer) {
	    c = '<';
	}
	yp_stuff_input(c, TRUE, FALSE);
	wait_buffer = FALSE;
	return(FALSE);
    }

    if (!literal_next) {
	/*
	 * This switch is for special characters; we skip over
	 * it for normal characters, or for literal-next mode.
	 */
	switch (c) {
	case CTRL('@'):
	    /*
	     * If ^@ is the first character inserted, insert the last
	     * text we inserted and return to command mode.
	     */
	    if (insert ? (curpos->p_line == Insertloc.p_line &&
		          curpos->p_index == Insertloc.p_index)
		       : (repstate == overwrite && flexempty(&Insbuff))) {
		yp_stuff_input('<', TRUE, FALSE);
		stuff("%c", ESC);
	    } else {
		/*
		 * Xvi can't handle NULs in files because it
		 * stores Lines as nul-terminated strings.
		 */
		beep();
	    }
	    return(FALSE);

	/*
	 * TOS doesn't seem to have a keyboard interrupt so keep the old
	 * code that make Ctrl-C do the same on TOS as everywhere else.
	 * POSIX for Unices says nothing special about Ctrl-C in vi.
	 */
#ifndef UNIX
	case CTRL('C'):
#endif
	case ESC:		/* An escape or Interrupt ends input mode */
	case_kbdintr_ch:	/* A label! */
	    if (!insert) {
		end_replace(c);
		return(TRUE);
	    }

	    /*
	     * If a prefix count was given, and we have not yet
	     * done that many insertions, re-insert the current
	     * text and go back to the start. Don't forget to
	     * clear the insert buffer - this is not quite right
	     * but will do for now.
	     */
	    if (Ins_repeat > 0) {
		stuff("%s%c", flexgetstr(&Insbuff), ESC);
		flexclear(&Insbuff);
		Ins_repeat--;
		return(FALSE);
	    }

	    curwin->w_set_want_col = TRUE;

	    /*
	     * If there is only auto-indentation
	     * on the current line, delete it.
	     */
	    if (curpos->p_index == indentchars &&
		curpos->p_line->l_text[indentchars] == '\0') {
		replchars(curpos->p_line, 0, indentchars, "");
		begin_line(FALSE);
	    }
	    indentchars = 0;

	    /*
	     * The cursor should end up on the last inserted
	     * character. This is an attempt to match the real
	     * 'vi', but it may not be quite right yet.
	     */
	    while (xvMoveLeft(curpos, FALSE) &&
				gchar(curwin->w_cursor) == '\0') {
		;
	    }

	    State = NORMAL;

	    end_command();

	    (void) yank_str('<', flexgetstr(&Insbuff), FALSE);
	    flexclear(&Insbuff);

	    if (!(echo & e_CHARUPDATE)) {
		echo |= e_CHARUPDATE;
		move_window_to_cursor();
		cursupdate();
	    }
	    xvUpdateAllBufferWindows();
	    return(TRUE);

	case CTRL('T'):
	case CTRL('D'):
	    if (!insert) {
		break;
	    }

	    /*
	     * If the only character on the line so far is a '0',
	     * backspace over it and delete the entire indent.
	     * If it's a '^', delete the indent for this line only
	     * and autoindent the next line the same as the previous one.
	     */
	    if (c == CTRL('D') && curpos->p_index == indentchars + 1) {
		Posn	pos;
		char	gc;

		pos = *curpos;
		(void) dec(&pos);
		gc = gchar(&pos);
		if (gc == '0' || gc == '^') {
		    if (gc == '^') next_indent = get_indent(curpos->p_line);
		    replchars(curpos->p_line, 0, indentchars + 1, "");
		    indentchars = set_indent(curpos->p_line, 0);
		    move_cursor(curpos->p_line, 0);
		    cursupdate();
		    updateline(nlines != plines(curpos->p_line));
		    (void) flexaddch(&Insbuff, c);
		    return(TRUE);
		}
	    }

	    /*
	     * If we're at the beginning of the line, or just
	     * after autoindent characters, move one shiftwidth
	     * left (CTRL('D')) or right (CTRL('T')).
	     */
	    if (curpos->p_index <= indentchars && Pn(P_shiftwidth) > 0) {
		Line	*lp;
		int	ind;
		int	sw = Pn(P_shiftwidth);

		lp = curpos->p_line;
		ind = get_indent(curpos->p_line);
		/*
		 * Don't just move forward or back by sw spaces;
		 * move to the next or last exact multiple of sw.
		 */
		if (c == CTRL('D')) {
		    ind = ((ind - 1) / sw) * sw;
		} else {
		    ind += sw - (ind % sw);
		}
		indentchars = set_indent(lp, ind);
		move_cursor(curpos->p_line, indentchars);
		cursupdate();
		updateline(nlines != plines(curpos->p_line));
		(void) flexaddch(&Insbuff, c);
	    } else {
		beep();
	    }
	    return(TRUE);

	case '\b':
	case DEL:
	    if (insert) {
		/*
		 * Can't backup past starting point.
		 */
		if (curpos->p_line == Insertloc.p_line &&
				curpos->p_index <= Insertloc.p_index) {
		    beep();
		    return(TRUE);
		}

		/*
		 * Can't backup to a previous line.
		 */
		if (curpos->p_line != Insertloc.p_line && curpos->p_index <= 0) {
		    beep();
		    return(TRUE);
		}
		(void) one_left(FALSE);
		if (curpos->p_index < indentchars)
		    indentchars--;
		replchars(curpos->p_line, curpos->p_index, 1, "");
		(void) flexaddch(&Insbuff, '\b');
		cursupdate();
		/*
		 * Make sure backspacing over a physical line
		 * break updates the screen correctly.
		 */
		updateline(curwin->w_col == 0);
		return(TRUE);
	    } else {		/* replace */
		if (repstate == overwrite && curwin->w_virtcol > start_column) {
		    (void) one_left(FALSE);
		    replchars(curpos->p_line,
				    curpos->p_index, 1,
				    (curpos->p_index < nchars) ?
				    mkstr(saved_line[curpos->p_index]) : "");
		    updateline(FALSE);
		    (void) flexaddch(&Insbuff, '\b');
		} else {
		    beep();
		    if (repstate == replace_one) {
			end_replace(c);
		    }
		}
		return(TRUE);
	    }

	case CTRL('U'):
	case CTRL('W'):
	    if (!insert) {
		/* Should implement ^U in Replace mode */
		break;
	    } else {
		/* Number of screen lines occupied by current line */
		int plines_before, plines_after;

		/* Where to start cancelling from in the line */
		int from_index;

		if (c == CTRL('U')) {
		    /*
		     * Cancel current line of inserted text.
		     * If we are on the same line as we started inserting on,
		     * cancel back to where the insertion started, otherwise
		     * we have inserted more than one logical line so
		     * cancel all text on the current line.
		     */
		    if (curpos->p_line == Insertloc.p_line) {
			from_index = Insertloc.p_index;
		    } else {
			from_index = 0;
		    }
		} else {
		    /* CTRL('W'): Delete back one word in inserted text */
		    Posn *new = bck_word(curpos, 0, TRUE);
		    if (new == NULL) return(TRUE);
		    from_index = new->p_index;
		    /* Don't delete over the insert position */
		    if (curpos->p_line == Insertloc.p_line &&
			from_index < Insertloc.p_index) {
			    from_index = Insertloc.p_index;
		    }
		}

		/*
		 * Leave auto-inserted indent characters or those added with
		 * ^T.
		 */
		if (from_index < indentchars && Pb(P_autoindent)) {
		    from_index = indentchars;
		}

		/* Nothing to cancel? */
		if (curpos->p_index <= from_index) {	/* == */
		    return(TRUE);
		}

		plines_before = plines(curpos->p_line);

		replchars(curpos->p_line, from_index,
			  curpos->p_index - from_index, "");

		plines_after = plines(curpos->p_line);

		curpos->p_index = from_index;

		(void) flexaddch(&Insbuff, c);
		cursupdate();
		/*
		 * Make sure cancelling over a physical line
		 * break updates the screen correctly.
		 */
		updateline(plines_before != plines_after);
		return(TRUE);
	    }

	case '\r':			/* new line */
	case '\n':
	    if (insert) {
		int	i;
		int	previndex;
		Line	*prevline;

		(void) flexaddch(&Insbuff, '\n');

		i = indentchars;
		prevline = curpos->p_line;
		previndex = curpos->p_index;

		if (openfwd(curpos, TRUE) == FALSE) {
		    stuff("%c", ESC);
		    show_error(out_of_memory);
		    return(TRUE);
		}

		/*
		 * If the previous line only had
		 * auto-indent on it, delete it.
		 */
		if (i == previndex) {
		    replchars(prevline, 0, i, "");
		}

		move_window_to_cursor();
		cursupdate();
		updateline(TRUE);
	    } else {		/* replace */
		if (curpos->p_line->l_next == curbuf->b_lastline &&
						repstate == overwrite) {
		    /*
		     * Don't allow splitting of last line of
		     * buffer in overwrite mode. Why not?
		     */
		    beep();
		    return(TRUE);
		}

		if (repstate == replace_one) {
		    echo &= ~e_CHARUPDATE;

		    /*
		     * First remove the character which is being replaced.
		     */
		    replchars(curpos->p_line, curpos->p_index, 1, "");

		    /*
		     * Then split the line at the current position.
		     */
		    if (openfwd(curpos, TRUE) == FALSE) {
			show_error(out_of_memory);
			return(TRUE);
		    }

		    (void) flexaddch(&Insbuff, c);

		    repstate = got_one;
		    end_replace('\n');

		} else {
		    (void) flexaddch(&Insbuff, '\n');

		    if (xvMoveDown(curwin->w_cursor, 1L, FALSE)) {
			info_update();

			/*
			 * This is wrong, but it's difficult
			 * to get it right.
			 */
			xvMoveToColumn(curwin->w_cursor, start_column);
		    }

		    free(saved_line);
		    saved_line = strsave(curpos->p_line->l_text);
		    if (saved_line == NULL) {
			State = NORMAL;
			return(TRUE);
		    }
		    nchars = strlen(saved_line);
		}
	    }
	    return(TRUE);

	case CTRL('B'):
	    if (insert || repstate == overwrite) {
		wait_buffer = TRUE;
		return(FALSE);
	    }
	    break;

	case CTRL('Q'):
	case CTRL('V'):
	    (void) flexaddch(&Insbuff, c);
	    literal_next = TRUE;
	    c = '^';
	    break;

	/*
	 * F1 doesn't pop up help in edit mode (maybe it should!)
	 * so map it to #1.
	 */
	case K_HELP:
	    stuff_to_map("#1");
	    return(FALSE);

	/*
	 * They can insert our internal codes for special keys by pressing,
	 * e.g., Left-Arrow to get \206.
	 * However, if we mask those here, they then can't insert some
	 * accented chars on MSDOS (e.g. a grave \206) nor UTF-8 chars
	 * whose second char is 0x80-0x8b (e.g. AE ligature \303\206)
	 */
	}
    } else {
	/*
	 * Insert a literal character
	 */

	/* Get rid of the ^ that we inserted to show the ^V */
	if (insert) {
	    replchars(curpos->p_line, curpos->p_index, 1, "");
	}
	/* Remove the phantom ^ from the insert buffer */
	flexrmchar(&Insbuff);
        literal_next = FALSE;
    }

    /*
     * If we get here, we want to insert the character into the buffer.
     */

    /*
     * Put the character into the insert buffer.
     */
    (void) flexaddch(&Insbuff, c);

    if (!insert) {	/* replace mode */
	if (repstate == overwrite || repstate == replace_one) {
	    int nlines = plines(curwin->w_cursor->p_line);

	    replchars(curpos->p_line, curpos->p_index, 1, mkstr(c));
	    updateline(nlines != plines(curwin->w_cursor->p_line));
	    if (!literal_next) {
		(void) one_right(TRUE);
	    }
	}
	/*
	 * If command was an 'r', leave replace mode after one character.
	 */
	if (!literal_next && repstate == replace_one) {
	    repstate = got_one;
	    end_replace(c);
	}

	return(TRUE);	/* Return from replace mode */
    }

    /* Insert mode */

    /*
     * Do the actual insertion of the new character.
     */
    replchars(curpos->p_line, curpos->p_index, 0, mkstr(c));

    /*
     * Deal with wrapmargin.
     * If literal_next, we are inserting the ^ that acknowledges a ^V
     * and wrapmargin will be done when they insert the actual char.
     */
    beginline = FALSE;
    if (!literal_next && Pn(P_wrapmargin) != 0 &&
		curwin->w_virtcol >= curwin->w_ncols - Pn(P_wrapmargin)) {
	register int	wspos;
	register int	nwspos;
	register char	*cltp;

	/*
	 * If we're trying inserting a space character, it's going to
	 * be deleted after the wrapping has been done, which is OK
	 * except that the new cursor position will be off by 1; so we
	 * make sure it's right by just going to the beginning of the
	 * new line.
	 */
	if (is_space(c))
	    beginline = TRUE;
	/*
	 * Scan back along the current line looking for a contiguous
	 * sequence of whitespace characters we can change to a
	 * newline to effect wrapping.
	 */
	cltp = curpos->p_line->l_text;
	for (wspos = nwspos = curpos->p_index; wspos > 0; --wspos) {
	    register int c;

	    c = cltp[wspos];
	    if (is_space(c)) {
		while (wspos > 0 && (c = cltp[wspos - 1]) != '\0'
		    && is_space(c)) {
		    --wspos;
		}
		break;
	    } else {
		nwspos = wspos;
	    }
	}
	if (wspos > 0) {
	    Posn	target;
	    int		offset;

	    target.p_line = curpos->p_line;
	    target.p_index = wspos;
	    offset = curpos->p_index - nwspos;
	    if (openfwd(&target, TRUE) == FALSE) {
		show_error(out_of_memory);
	    } else {
		int	newindex = curpos->p_index;
		Line	*newlp = curpos->p_line;
		register int c;

		/*
		 * Have split the line - now delete any leading
		 * non-autoindent whitespace characters on the new
		 * line.
		 */
		while ((c = newlp->l_text[newindex]) != '\0' && is_space(c))
		    replchars(newlp, newindex, 1, "");
		move_cursor(newlp, newindex + offset);
	    }
	}
    }

    /*
     * Update the screen.
     */
    s_inschar(c);
    updateline(nlines != plines(curpos->p_line));

    /*
     * If showmatch mode is set, check for right parens
     * and braces. If there isn't a match, then beep.
     * If there is a match AND it's on the screen,
     * flash to it briefly.
     *
     * These characters included to make this
     * source file work okay with showmatch: [ { (
     */
    if (Pb(P_showmatch) && (c == ')' || c == '}' || c == ']')) {
	Posn	*lpos, csave;

	lpos = showmatch();
	if (lpos == NULL) {
	    beep();
	} else if (!earlier(lpos->p_line, curwin->w_topline) &&
		       earlier(lpos->p_line, curwin->w_botline)) {
	    /*
	     * Show the match if it's on screen.
	     */
	    xvUpdateAllBufferWindows();

	    csave = *curpos;
	    move_cursor(lpos->p_line, lpos->p_index);
	    cursupdate();
	    wind_goto();
	    VSflush(curwin->w_vs);

	    Wait200ms();

	    move_cursor(csave.p_line, csave.p_index);
	    cursupdate();
	}
    }

    /*
     * Finally, move the cursor right one space, or to the beginning
     * of the line if required.
     */
    if (beginline) {
	begin_line(TRUE);
    } else {
	/* If we're showing the ^ for a ^V, leave the cursor on the ^ */
	if (!literal_next) {
	    (void) one_right(TRUE);
	}
    }

    return(TRUE);
}

/*
 * This function is the interface provided for functions in
 * normal mode to go into insert mode. We only come out of
 * insert mode when the user presses escape.
 *
 * The parameter is a flag to say whether we should start
 * at the start of the line.
 *
 * Note that we do not have to call start_command() as the
 * caller does that for us - this is so the caller can include
 * any other stuff (e.g. an initial delete) into the command.
 */
void
startinsert(startln, repeat)
bool_t	startln;	/* if set, insert point really at start of line */
int	repeat;		/* number of times to repeat the insertion */
{
    Insertloc = *curwin->w_cursor;
    if (startln)
	Insertloc.p_index = 0;
    flexclear(&Insbuff);
    Ins_repeat = repeat;
    next_indent = 0;
    State = INSERT;
}

/*
 * This function is the interface provided for functions in
 * normal mode to go into replace mode. We only come out of
 * replace mode when the user presses escape, or when they
 * used the 'r' command and typed a single character.
 *
 * The parameter is the command character which took us into replace mode.
 */
void
startreplace(cmd)
Cmd *	cmd;
{
    int c = cmd->cmd_ch1;
    int repeat = IDEF1(cmd->cmd_prenum) - 1;

    /*
     * 'r' on an empty line or 'Nr' when there are less than N characters
     * under and after the cursor, is an error.
     */
    if (c == 'r') {
	Posn *curp = curwin->w_cursor;
	char *line = curp->p_line->l_text + curp->p_index;

	if (repeat + 1 > strlen(line)) {
	    beep();
	    /* Failed commands interrupt redos and mapped commands */
	    unstuff();
	    return;
	}
    }

    if (!start_command(cmd)) {
	return;
    }
    start_index = curwin->w_cursor->p_index;
    start_column = curwin->w_virtcol;

    if (c == 'r') {
	repstate = replace_one;
	saved_line = NULL;
    } else {
	repstate = overwrite;
	saved_line = strsave(curwin->w_cursor->p_line->l_text);
	if (saved_line == NULL) {
	    beep();
	    end_command();
	    return;
	}
	nchars = strlen(saved_line);

	/*
	 * Initialize Insbuff. Note that we don't do this if the
	 * command was 'r', because they might type ESC to abort
	 * the command, in which case we shouldn't change Insbuff.
	 */
	flexclear(&Insbuff);
    }
    Ins_repeat = repeat;
    State = REPLACE;
}

static void
end_replace(c)
    int	c;
{
    /*
     * Handle repeat counts - but not if they typed 'r' and then ESC
     * straight away. In this case, repstate will still be replace_one.
     */
    if (Ins_repeat > 0 && repstate != replace_one) {
	if (repstate == got_one) {
	    repstate = replace_one;
	    stuff("%c", c);
	} else if (repstate == overwrite) {
	    stuff("%s%c", flexgetstr(&Insbuff), ESC);
	}
	flexclear(&Insbuff);
	Ins_repeat--;
	return;
    }

    State = NORMAL;
    end_command();

    indentchars = 0;	/* must reset this before leaving replace mode */

    /*
     * If (repstate == replace_one), they must have typed 'r', then
     * thought better of it & typed ESC; so we shouldn't complain or
     * change the buffer, the cursor position, or Insbuff.
     */
    if (repstate != replace_one) {

	(void) yank_str('<', flexgetstr(&Insbuff), FALSE);
	flexclear(&Insbuff);

	/*
	 * Free the saved line if necessary.
	 */
	if (repstate == overwrite) {
	    free(saved_line);
	}

	/*
	 * The cursor should end up on the
	 * last replaced character.
	 */
	while (one_left(FALSE) && gchar(curwin->w_cursor) == '\0') {
	    ;
	}

	if (!(echo & e_CHARUPDATE)) {
	    echo |= e_CHARUPDATE;
	    move_window_to_cursor();
	    cursupdate();
	}
	xvUpdateAllBufferWindows();
    }
}

char *
mkstr(c)
int	c;
{
    static	char	s[2];

    s[0] = c;
    s[1] = '\0';

    return(s);
}
