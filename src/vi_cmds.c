/* Copyright (c) 1990,1991,1992,1993,1994 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    vi_cmds.c
* module function:
    Most vi commands are implemented in this file. See normal.c
    for the switching code which determines which functions are
    called for which commands.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

void
do_cmd(cmd)
Cmd	*cmd;
{
    switch (cmd->cmd_ch1) {
    case K_HELP:
	exHelp(curwin);
	break;

    case CTRL('R'):
    case CTRL('L'):
	redraw_all(curwin, TRUE);
	break;

    case CTRL('G'):
	show_file_info(curwin, TRUE);
	break;

    case CTRL(']'):		/* :ta to current identifier */
	tagword();
	break;

    /*
     * Some convenient abbreviations...
     */
    case 'D':
	stuff("\"%cd$", cmd->cmd_yp_name);
	break;

    case 'Y':
	stuff("\"%c%dyy", cmd->cmd_yp_name, IDEF1(cmd->cmd_prenum));
	break;

    case 'C':
	stuff("\"%cc$", cmd->cmd_yp_name);
	break;

    case 'S':
    	stuff("\"%c%dcc", cmd->cmd_yp_name, IDEF1(cmd->cmd_prenum));
	break;

    case 'p':
    case 'P':
	Redo.r_mode = r_normal;
	do_put(curwin, curwin->w_cursor,
		(cmd->cmd_ch1 == 'p') ? FORWARD : BACKWARD, cmd->cmd_yp_name);
	if (is_digit(cmd->cmd_yp_name) && cmd->cmd_yp_name != '0' && cmd->cmd_yp_name != '9') {
	    cmd->cmd_yp_name++;
	}
	flexclear(&Redo.r_fb);
	(void) lformat(&Redo.r_fb, "\"%c%d%c", cmd->cmd_yp_name,
			IDEF1(cmd->cmd_prenum), cmd->cmd_ch1);
	break;

    case 's':		/* substitute characters */
	if (start_command(curwin)) {
	    replchars(curwin, curwin->w_cursor->p_line,
			curwin->w_cursor->p_index, IDEF1(cmd->cmd_prenum), "");
	    updateline(curwin);
	    Redo.r_mode = r_insert;
	    flexclear(&Redo.r_fb);
	    (void) lformat(&Redo.r_fb, "%ds", IDEF1(cmd->cmd_prenum));
	    startinsert(FALSE, 0);
	}
	break;

    case ':':
    case '?':
    case '/':
	cmd_init(curwin, cmd->cmd_ch1);
	break;

    case '&':
	(void) exAmpersand(curwin, curwin->w_cursor->p_line,
				    curwin->w_cursor->p_line, "");
	begin_line(curwin, TRUE);
	updateline(curwin);
	break;

    case 'R':
    case 'r':
	Redo.r_mode = (cmd->cmd_ch1 == 'r') ? r_replace1 : r_insert;
	flexclear(&Redo.r_fb);
	(void) flexaddch(&Redo.r_fb, cmd->cmd_ch1);
	startreplace(cmd->cmd_ch1, IDEF1(cmd->cmd_prenum) - 1);
	break;

    case 'J':
    {
	int	count;
	int	size1;		/* chars in the first line */
	Line *	line = curwin->w_cursor->p_line;

	if (!start_command(curwin)) {
	    beep(curwin);
	    break;
	}

	size1 = strlen(line->l_text);
	count = IDEF1(cmd->cmd_prenum) - 1; /* 4J does 3 Joins to join 4 lines */
	do {				    /* but 0J and 1J do one, like 2J */
	    if (!xvJoinLine(curwin, line, FALSE)) {
		beep(curwin);
		break;
	    }
	} while (--count > 0);

	/* Leave the cursor on the first char after the first of
	 * the joined lines, which is usually the inserted space,
	 * with a special case for when we join a line to nothing.
	 */
	if (line->l_text[size1] == '\0') size1--;
	move_cursor(curwin, line, size1);

	xvUpdateAllBufferWindows(curbuf);

	end_command(curwin);

	Redo.r_mode = r_normal;
	flexclear(&Redo.r_fb);
	(void) flexaddch(&Redo.r_fb, cmd->cmd_ch1);
	break;
    }

    case K_CGRAVE:			/* shorthand command */
#ifndef	QNX
    /*
     * We can't use this key on QNX.
     */
    case CTRL('^'):
#endif
	exEditAlternateFile(curwin);
	break;

    case 'u':
    case K_UNDO:
	undo(curwin, FALSE, 0);
	break;

    case CTRL('Z'):			/* suspend editor */
	exSuspend(curwin);
	break;

    /*
     * Buffer handling.
     */
    case CTRL('T'):			/* shrink window */
	xvResizeWindow(curwin, - IDEF1(cmd->cmd_prenum));
	move_cursor_to_window(curwin);
	break;

    case CTRL('W'):			/* grow window */
	xvResizeWindow(curwin, IDEF1(cmd->cmd_prenum));
	break;

    case CTRL('O'):	/* make window as large as possible */
	xvResizeWindow(curwin, INT_MAX);
	break;

    case 'g':
	/*
	 * Move the cursor to the next window that is displayed.
	 */
	curwin = xvNextDisplayedWindow(curwin);
	xvUseWindow(curwin);
	curbuf = curwin->w_buffer;
	move_cursor_to_window(curwin);
	cursupdate(curwin);
	wind_goto(curwin);
	break;

    case '@':
	yp_stuff_input(curwin, cmd->cmd_ch2, TRUE);
	break;

    /*
     * Marks
     */
    case 'm':
	if (!setmark(cmd->cmd_ch2, curbuf, curwin->w_cursor))
	    beep(curwin);
	break;

    case 'Z':		/* write, if changed, and exit */
	if (cmd->cmd_ch2 != 'Z') {
	    beep(curwin);
	    break;
	}

	/*
	 * Make like a ":x" command.
	 */
	exXit(curwin);
	break;

    case '.':
	/*
	 * '.', meaning redo. As opposed to '.' as a target.
	 */
	stuff("%s", flexgetstr(&Redo.r_fb));
	if (Redo.r_mode != r_normal) {
	    yp_stuff_input(curwin, '<', TRUE);
	    if (Redo.r_mode == r_insert) {
		stuff("%c", ESC);
	    }
	}
	break;

    default:
	beep(curwin);
	break;
    }
}

/*
 * Handle page motion (control-F or control-B).
 */
void
do_page(cmd)
Cmd	*cmd;
{
    long		overlap;
    long		n;

    /*
     * First move the cursor to the top of the screen
     * (for ^B), or to the top of the next screen (for ^F).
     */
    move_cursor(curwin, (cmd->cmd_ch1 == CTRL('B')) ?
		   curwin->w_topline : curwin->w_botline, 0);

    /*
     * Cursor could have moved to the lastline of the buffer,
     * if the window is at the end of the buffer. Disallow
     * the cursor from being outside the buffer's bounds.
     */
    if (curwin->w_cursor->p_line == curbuf->b_lastline) {
	move_cursor(curwin, curbuf->b_lastline->l_prev, 0);
    }

    /*
     * Decide on the amount of overlap to use.
     */
    if (curwin->w_nrows > 10) {
	/*
	 * At least 10 text lines in window.
	 */
	overlap = 2;
    } else if (curwin->w_nrows > 3) {
	/*
	 * Between 3 and 9 text lines in window.
	 */
	overlap = 1;
    } else {
	/*
	 * 1 or 2 text lines in window.
	 */
	overlap = 0;
    }

    /*
     * Given the overlap, decide where to move the cursor;
     * this will determine the new top line of the screen.
     */
    if (cmd->cmd_ch1 == CTRL('F')) {
	n = - overlap;
	n += (LDEF1(cmd->cmd_prenum) - 1) * (curwin->w_nrows - overlap - 1);
    } else {
	n = (- LDEF1(cmd->cmd_prenum)) * (curwin->w_nrows - overlap - 1);
    }

    if (n > 0) {
	(void) xvMoveDown(curwin->w_cursor, n);
    } else {
	(void) xvMoveUp(curwin->w_cursor, -n);
    }

    /*
     * Redraw the screen with the cursor at the top.
     */
    begin_line(curwin, TRUE);
    curwin->w_topline = curwin->w_cursor->p_line;
    redraw_window(curwin, FALSE);

    if (cmd->cmd_ch1 == CTRL('B')) {
	/*
	 * And move it to the bottom.
	 */
	move_window_to_cursor(curwin);
	cursupdate(curwin);
	move_cursor(curwin, curwin->w_botline->l_prev, 0);
	begin_line(curwin, TRUE);
    }

    /*
     * Finally, show where we are in the file.
     */
    show_file_info(curwin, TRUE);
}

void
do_scroll(cmd)
Cmd	*cmd;
{
    switch (cmd->cmd_ch1) {
    case CTRL('D'):
	scrollup(curwin, curwin->w_nrows / 2);
	if (xvMoveDown(curwin->w_cursor, (long) (curwin->w_nrows / 2))) {
	    info_update(curwin);
	    xvMoveToColumn(curwin->w_cursor, curwin->w_curswant);
	}
	break;

    case CTRL('U'):
	scrolldown(curwin, curwin->w_nrows / 2);
	if (xvMoveUp(curwin->w_cursor, (long) (curwin->w_nrows / 2))) {
	    info_update(curwin);
	    xvMoveToColumn(curwin->w_cursor, curwin->w_curswant);
	}
	break;

    case CTRL('E'):
	scrollup(curwin, (unsigned) IDEF1(cmd->cmd_prenum));
	break;

    case CTRL('Y'):
	scrolldown(curwin, (unsigned) IDEF1(cmd->cmd_prenum));
	break;
    }

    redraw_window(curwin, FALSE);
    move_cursor_to_window(curwin);
}

/*
 * Handle adjust window command ('z').
 */
void
do_z(cmd)
Cmd	*cmd;
{
    Line	*lp;
    int	l;
    int	znum;

    switch (cmd->cmd_ch2) {
    case '\n':				/* put cursor at top of screen */
    case '\r':
	znum = 1;
	break;

    case '.':				/* put cursor in middle of screen */
	znum = curwin->w_nrows / 2;
	break;

    case '-':				/* put cursor at bottom of screen */
	znum = curwin->w_nrows - 1;
	break;

    default:
	return;
    }

    if (cmd->cmd_prenum > 0) {
	xvMoveToLineNumber(cmd->cmd_prenum);
    }
    lp = curwin->w_cursor->p_line;
    for (l = 0; l < znum && lp != curbuf->b_line0; ) {
	l += plines(curwin, lp);
	curwin->w_topline = lp;
	lp = lp->l_prev;
    }
    move_cursor_to_window(curwin);	/* just to get cursupdate to work */
    cursupdate(curwin);
    redraw_window(curwin, FALSE);
}

/*
 * Handle character delete commands ('x' or 'X').
 */
void
do_x(cmd)
Cmd	*cmd;
{
    Posn	*curp;
    Posn	lastpos;
    int		nchars;
    int		i;

    nchars = IDEF1(cmd->cmd_prenum);
    Redo.r_mode = r_normal;
    flexclear(&Redo.r_fb);
    (void) lformat(&Redo.r_fb, "%d%c", nchars, cmd->cmd_ch1);
    curp = curwin->w_cursor;

    if (cmd->cmd_ch1 == 'X') {
	for (i = 0; i < nchars && one_left(curwin, FALSE); i++)
	    ;
	nchars = i;
	if (nchars == 0) {
	    beep(curwin);
	    return;
	}

    } else /* cmd->cmd_ch1 == 'x' */ {
	char	*line;

	/*
	 * Ensure that nchars is not too big.
	 */
	line = curp->p_line->l_text + curp->p_index;
	for (i = 0; i < nchars && line[i] != '\0'; i++)
	    ;
	nchars = i;

	if (curp->p_line->l_text[0] == '\0') {
	    /*
	     * Can't do it on a blank line.
	     */
	    beep(curwin);
	    return;
	}
    }

    lastpos.p_line = curp->p_line;
    lastpos.p_index = curp->p_index + nchars - 1;
    yp_push_deleted();
    (void) do_yank(curbuf, curp, &lastpos, TRUE, cmd->cmd_yp_name);
    replchars(curwin, curp->p_line, curp->p_index, nchars, "");
    if (curp->p_line->l_text[curp->p_index] == '\0') {
	(void) one_left(curwin, FALSE);
    }
    updateline(curwin);
}

/*
 * Handle '~' and CTRL('_') commands.
 */
void
do_rchar(cmd)
Cmd	*cmd;
{
    Posn	*cp;
    char	*tp;
    int		c;
    char	newc[2];

    Redo.r_mode = r_normal;
    flexclear(&Redo.r_fb);
    (void) flexaddch(&Redo.r_fb, cmd->cmd_ch1);
    cp = curwin->w_cursor;
    tp = cp->p_line->l_text;
    if (tp[0] == '\0') {
	/*
	 * Can't do it on a blank line.
	 */
	beep(curwin);
	return;
    }
    c = tp[cp->p_index];

    switch (cmd->cmd_ch1) {
    case '~':
	newc[0] = is_alpha(c) ?
		    is_lower(c) ?
		    	to_upper(c)
			: to_lower(c)
		    : c;
	break;
    case CTRL('_'):			/* flip top bit */
#ifdef	TOP_BIT
	newc[0] = c ^ TOP_BIT;
	if (newc[0] == '\0') {
	    newc[0] = c;
	}
#else	/* not TOP_BIT */
	beep(curwin);
	return;
#endif	/* TOP_BIT */
    }

    newc[1] = '\0';
    replchars(curwin, cp->p_line, cp->p_index, 1, newc);
    updateline(curwin);
    (void) one_right(curwin, FALSE);
}

/*
 * Handle commands which just go into insert mode
 * ('i', 'a', 'I', 'A', 'o', 'O').
 */
void
do_ins(cmd)
Cmd	*cmd;
{
    bool_t	startpos = TRUE;	/* FALSE means start position moved */

    if (!start_command(curwin)) {
	return;
    }

    Redo.r_mode = r_insert;
    flexclear(&Redo.r_fb);
    (void) flexaddch(&Redo.r_fb, cmd->cmd_ch1);

    switch (cmd->cmd_ch1) {
    case 'o':
    case 'O':
	if (
	    (
		(cmd->cmd_ch1 == 'o') ?
		    openfwd(curwin, curwin->w_cursor, FALSE)
		    :
		    openbwd()
	    ) == FALSE
	) {
	    beep(curwin);
	    end_command(curwin);
	    return;
	}
	break;

    case 'I':
	begin_line(curwin, TRUE);
	break;

    case 'A':
	while (one_right(curwin, TRUE))
	    ;
	break;

    case 'a':
	/*
	 * 'a' works just like an 'i' on the next character.
	 */
	(void) one_right(curwin, TRUE);
    }

    switch (cmd->cmd_ch1) {
    case 'A':
    case 'I':
    case 'a':
    case 'i':
	startpos = FALSE;
    }

    startinsert(startpos, IDEF1(cmd->cmd_prenum) - 1);
}
