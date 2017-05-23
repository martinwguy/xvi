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
	exHelp();
	break;

    case CTRL('R'):
    case CTRL('L'):
	redraw_all(TRUE);
	break;

    case CTRL('G'):
	show_file_info(TRUE);
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
	do_put(curwin->w_cursor, (cmd->cmd_ch1 == 'p') ? FORWARD : BACKWARD,
	       cmd->cmd_yp_name, cmd);
	/*
	 * This lets you to say "1P..." to recover the most recent deletions
	 * in order, like saying "1P"2P"3P"4P...
	 */
	if (is_digit(cmd->cmd_yp_name)
	     && cmd->cmd_yp_name != '0' && cmd->cmd_yp_name != '9') {
	    cmd->cmd_yp_name++;
	}
	flexclear(&Redo.r_fb);
	(void) lformat(&Redo.r_fb, "\"%c%d%c", cmd->cmd_yp_name,
			IDEF1(cmd->cmd_prenum), cmd->cmd_ch1);
	break;

    case 's':		/* substitute characters */
	if (start_command(cmd)) {
	    int nlines = plines(curwin->w_cursor->p_line);

	    replchars(curwin->w_cursor->p_line,
			curwin->w_cursor->p_index, IDEF1(cmd->cmd_prenum), "");
	    updateline(nlines != plines(curwin->w_cursor->p_line));
	    Redo.r_mode = r_insert;
	    flexclear(&Redo.r_fb);
	    (void) lformat(&Redo.r_fb, "%ds", IDEF1(cmd->cmd_prenum));
	    startinsert(FALSE, 0);
	}
	break;

    case ':':
    case '?':
    case '/':
	cmd_init(cmd->cmd_ch1);
	break;

    case '&':
	(void) exAmpersand(curwin->w_cursor->p_line,
			   curwin->w_cursor->p_line, "");
	begin_line(TRUE);
	updateline(FALSE);
	break;

    case 'R':
    case 'r':
	Redo.r_mode = (cmd->cmd_ch1 == 'r') ? r_replace1 : r_insert;
	flexclear(&Redo.r_fb);
	if (!flexaddch(&Redo.r_fb, cmd->cmd_ch1)) break;
	startreplace(cmd);
	break;

    case 'J':
    {
	int	count;
	int	size1;		/* chars in the first line */
	Line *	line = curwin->w_cursor->p_line;

	if (!start_command(cmd)) {
	    beep();
	    break;
	}

	size1 = strlen(line->l_text);
	count = IDEF1(cmd->cmd_prenum) - 1; /* 4J does 3 Joins to join 4 lines */
	do {				    /* but 0J and 1J do one, like 2J */
	    if (!xvJoinLine(line, FALSE)) {
		beep();
		break;
	    }
	} while (--count > 0);

	/* Leave the cursor on the first char after the first of
	 * the joined lines, which is usually the inserted space,
	 * with a special case for when we join a line to nothing.
	 */
	if (line->l_text[size1] == '\0') size1--;
	move_cursor(line, size1);

	xvUpdateAllBufferWindows();

	end_command();

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
	exEditAlternateFile();
	break;

    case 'u':
    case K_UNDO:
	undo();
	break;

    case 'U':
	undoline();
	break;

    case CTRL('Z'):			/* suspend editor */
	exSuspend(FALSE);
	break;

    /*
     * Buffer handling.
     */
    case CTRL('T'):			/* shrink window */
	xvResizeWindow(-IDEF1(cmd->cmd_prenum));
	move_cursor_to_window();
	break;

    case CTRL('W'):			/* grow window */
	xvResizeWindow(IDEF1(cmd->cmd_prenum));
	break;

    case CTRL('O'):	/* make window as large as possible */
	xvResizeWindow(INT_MAX);
	break;

    case 'g':
	/*
	 * Move the cursor to the next window that is displayed.
	 */
	set_curwin(xvNextDisplayedWindow(curwin));
	xvUseWindow();
	move_cursor_to_window();
	cursupdate();
	break;

    case '@':
	yp_stuff_input(cmd->cmd_ch2, TRUE, TRUE);
	break;

    /*
     * Marks
     */
    case 'm':
	if (!setmark(cmd->cmd_ch2, curbuf, curwin->w_cursor))
	    beep();
	break;

    case 'Z':		/* write, if changed, and exit */
	if (cmd->cmd_ch2 != 'Z') {
	    beep();
	    break;
	}

	/*
	 * Make like a ":x" command.
	 */
	if (!exXit()) {
	    unstuff();
	}
	break;

    case '.':
	/*
	 * '.', meaning redo. As opposed to '.' as a target.
	 */
	stuff("%s", flexgetstr(&Redo.r_fb));
	if (Redo.r_mode != r_normal) {
	    yp_stuff_input('<', TRUE, FALSE);
	    if (Redo.r_mode == r_insert) {
		stuff("%c", ESC);
	    }
	}
	break;

    default:
	beep();
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

    /* Map command aliases to canonical commands */
    switch (cmd->cmd_ch1) {
    case K_PGUP:
	cmd->cmd_ch1 = CTRL('B');
	break;
    case K_PGDOWN:
	cmd->cmd_ch1 = CTRL('F');
	break;
    }

    /*
     * First move the cursor to the top of the screen
     * (for ^B), or to the top of the next screen (for ^F).
     */
    move_cursor((cmd->cmd_ch1 == CTRL('B')) ?
		   curwin->w_topline : curwin->w_botline, 0);

    /*
     * Cursor could have moved to the lastline of the buffer,
     * if the window is at the end of the buffer. Disallow
     * the cursor from being outside the buffer's bounds.
     */
    if (curwin->w_cursor->p_line == curbuf->b_lastline) {
	move_cursor(b_last_line_of(curbuf), 0);
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
	(void) xvMoveDown(curwin->w_cursor, n, TRUE);
    } else {
	(void) xvMoveUp(curwin->w_cursor, -n, TRUE);
    }

    /*
     * Redraw the screen with the cursor at the top.
     */
    begin_line(TRUE);
    curwin->w_topline = curwin->w_cursor->p_line;
    redraw_window(FALSE);

    if (cmd->cmd_ch1 == CTRL('B')) {
	/*
	 * And move it to the bottom.
	 */
	move_window_to_cursor();
	cursupdate();
	move_cursor(curwin->w_botline->l_prev, 0);
	begin_line(TRUE);
    }

    /*
     * Finally, show where we are in the file.
     */
    show_file_info(TRUE);
}

void
do_scroll(cmd)
Cmd	*cmd;
{
    int scroll = Pn(P_scroll) > 0 ? Pn(P_scroll) : curwin->w_nrows / 2;

    switch (cmd->cmd_ch1) {
    case CTRL('D'):
	scrollup(scroll);
	if (xvMoveDown(curwin->w_cursor, (long) scroll, TRUE)) {
	    info_update();
	    xvMoveToColumn(curwin->w_cursor, curwin->w_curswant);
	}
	break;

    case CTRL('U'):
	scrolldown(scroll);
	if (xvMoveUp(curwin->w_cursor, (long) scroll, TRUE)) {
	    info_update();
	    xvMoveToColumn(curwin->w_cursor, curwin->w_curswant);
	}
	break;

    case CTRL('E'):
	scrollup((unsigned) IDEF1(cmd->cmd_prenum));
	break;

    case CTRL('Y'):
	scrolldown((unsigned) IDEF1(cmd->cmd_prenum));
	break;
    }

    redraw_window(FALSE);
    move_cursor_to_window();
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
	l += plines(lp);
	curwin->w_topline = lp;
	lp = lp->l_prev;
    }
    move_cursor_to_window();	/* just to get cursupdate to work */
    cursupdate();
    redraw_window(FALSE);
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
    int		nlines;

    nchars = IDEF1(cmd->cmd_prenum);
    Redo.r_mode = r_normal;
    flexclear(&Redo.r_fb);
    (void) lformat(&Redo.r_fb, "%d%c", nchars, cmd->cmd_ch1);
    curp = curwin->w_cursor;
    nlines = plines(curp->p_line);

    if (cmd->cmd_ch1 == 'X') {
	for (i = 0; i < nchars && one_left(FALSE); i++)
	    ;
	nchars = i;
	if (nchars == 0) {
	    beep();
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
	    beep();
	    return;
	}
    }

    lastpos.p_line = curp->p_line;
    lastpos.p_index = curp->p_index + nchars - 1;
    yp_push_deleted();
    (void) do_yank(curp, &lastpos, TRUE, cmd->cmd_yp_name);
    replchars(curp->p_line, curp->p_index, nchars, "");
    if (curp->p_line->l_text[curp->p_index] == '\0') {
	(void) one_left(FALSE);
    }
    updateline(nlines != plines(curp->p_line));
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
    int		count;

    Redo.r_mode = r_normal;
    flexclear(&Redo.r_fb);
    if (!flexaddch(&Redo.r_fb, cmd->cmd_ch1)) return;
    cp = curwin->w_cursor;
    tp = cp->p_line->l_text;
    if (tp[0] == '\0') {
	/*
	 * Can't do it on a blank line.
	 */
	beep();
	return;
    }
    count = IDEF1(cmd->cmd_prenum);
    start_command(cmd);
    do {
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
	    beep();
	    return;
#endif	/* TOP_BIT */
	}

	newc[1] = '\0';
	replchars(cp->p_line, cp->p_index, 1, newc);
	updateline(FALSE);
	if (!one_right(FALSE))
	    break;
    } while (--count > 0);
    end_command();
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

    if (!start_command(cmd)) {
	return;
    }

    Redo.r_mode = r_insert;
    flexclear(&Redo.r_fb);
    if (!flexaddch(&Redo.r_fb, cmd->cmd_ch1)) goto oom;

    switch (cmd->cmd_ch1) {
    case 'o':
    case 'O':
	if (
	    (
		(cmd->cmd_ch1 == 'o') ?
		    openfwd(curwin->w_cursor, FALSE)
		    :
		    openbwd()
	    ) == FALSE
	) {
oom:	    beep();
	    end_command();
	    return;
	}
	break;

    case 'I':
	begin_line(TRUE);
	break;

    case 'A':
	while (one_right(TRUE))
	    ;
	break;

    case 'a':
	/*
	 * 'a' works just like an 'i' on the next character.
	 */
	(void) one_right(TRUE);
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
