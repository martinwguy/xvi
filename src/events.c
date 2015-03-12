/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    events.c
* module function:
    Deals with incoming events.
    The main entry point for input to the editor.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

static	bool_t	n_proc P((int));
static	bool_t	c_proc P((int));
static	bool_t	d_proc P((int));

volatile int	keystrokes;

xvResponse *
xvi_handle_event(ev)
xvEvent	*ev;
{
    static xvResponse	resp;
    bool_t		do_update;
    int			c;

    switch (ev->ev_type) {
    case Ev_char:
	keystrokes++;
	map_char(ev->ev_inchar);
	break;

    case Ev_timeout:
	if (map_waiting()) {
	    map_timeout();
	} else if (keystrokes >= PSVKEYS) {
	    (void) exPreserveAllBuffers();
	    keystrokes = 0;
	}
	break;

    case Ev_refresh:
	/*redraw_all((Xviwin *) ev->ev_vs->pv_window, ev->ev_do_clear);*/
	redraw_all((Xviwin *) ev->ev_vs->pv_window, TRUE);
	break;

    case Ev_resize:
	{
	    Xviwin	*win;

	    win = (Xviwin *) ev->ev_vs->pv_window;
	    if (!vs_resize(ev->ev_vs, ev->ev_rows, ev->ev_columns)) {
		/*
		 * Oh shit. Can't allocate space for new window size.
		 * This is very difficult to fix...
		 */
		(void) exPreserveAllBuffers();
		show_error(win,
		    "Internal error: can't allocate space for new window size");
	    }
	    xvAdjustWindows(ev->ev_vs, (ev->ev_columns != 0));
	    move_cursor_to_window(curwin);
	    cursupdate(curwin);
	    wind_goto(curwin);	/* ensure cursor is where it should be */
	    break;
	}
	break;

    case Ev_mouseclick:
	mouseclick(ev->ev_m_row, ev->ev_m_col);
	break;

    case Ev_mousedrag:
	mousedrag(ev->ev_m_row, ev->ev_m_endrow, ev->ev_m_col, ev->ev_m_endcol);
	break;

    case Ev_mousemove:
	mousemove(ev->ev_m_row);
	break;

    case Ev_breakin:
	if (State == DISPLAY) {
	    map_char(CTRL('C'));
	} else {
	    /*
	     * We don't have to handle this any better; any code
	     * which is actually interruptible will check the
	     * kbdintr variable itself at appropriate points.
	     * So this code only gets executed when we get an
	     * interrupt in a non-interruptible code path, or
	     * when we aren't doing anything at all.
	     */
	    beep(curwin);
	}
	break;

    case Ev_suspend_request:
	/*
	 * We only allow editor suspension in top-level command state.
	 */
	if (State == NORMAL) {
	    exSuspend(curwin);
	} else if (State == SUBNORMAL) {
	    /*
	     * Treat ^Z as 2nd char of a two-char command as being an ESCAPE.
	     * This will cause a beep; a subsequent ^Z may then be issued to
	     * suspend the editor.
	     */
	    map_char(ESC);
	} else {
	    beep(curwin);
	}
	break;

    case Ev_terminate:
    case Ev_disconnected:
	/*
	 * Preserve modified buffers, and then exit.
	 */
	(void) exPreserveAllBuffers();
	State = EXITING;
	break;
    }

    /*
     * Look to see if the event produced any input characters
     * which we can feed into the editor. Call the appropriate
     * function for each one, according to the current State.
     */
    do_update = FALSE;
    while ((c = map_getc()) != EOF) {
	bool_t	(*func)P((int));

	switch (State) {
	case NORMAL:
	case SUBNORMAL:
	    func = n_proc;
	    break;

	case CMDLINE:
	    func = c_proc;
	    break;

	case DISPLAY:
	    func = d_proc;
	    break;

	case INSERT:
	    func = i_proc;
	    break;

	case REPLACE:
	    func = r_proc;
	    break;

	default: /* EXITING */
	    break;
	}
	if ((*func)(c)) {
	    do_update = TRUE;
	}

	/*
	 * Look at the resultant state, and the
	 * result of the proc() routine, to see
	 * whether to update the display.
	 */
	switch (State) {
	case CMDLINE:
	case DISPLAY:
	    break;

	case NORMAL:
	case SUBNORMAL:
	case INSERT:
	case REPLACE:
	    if (do_update) {
		move_window_to_cursor(curwin);
		cursupdate(curwin);
		wind_goto(curwin);
	    }
	    break;

	case EXITING:
	    break;
	}
    }

    if (State == EXITING) {
	resp.xvr_type = Xvr_exit;
	return(&resp);
    }

    if (kbdintr) {
	if (imessage) {
	    show_message(curwin, "Interrupted");
	    wind_goto(curwin);	/* put cursor back */
	}
	imessage = (kbdintr = 0);
    }

    if (map_waiting()) {
	resp.xvr_timeout = (long) Pn(P_timeout);
    } else if (keystrokes >= PSVKEYS) {
	resp.xvr_timeout = (long) Pn(P_preservetime) * 1000;
    } else {
	resp.xvr_timeout = 0;
    }
    resp.xvr_type = Xvr_timed_input;
    return(&resp);
}

/*
 * Process the given character in command mode.
 */
static bool_t
n_proc(c)
int	c;
{
    unsigned	savecho;
    bool_t	result;

    savecho = echo;
    result = normal(c);
    echo = savecho;
    return(result);
}

/*
 * Returns TRUE if screen wants updating, FALSE otherwise.
 */
static bool_t
c_proc(c)
int	c;
{
    char	*cmdline;
    char	*savedline;
    bool_t	retval = TRUE;

    switch (cmd_input(curwin, c)) {
    case cmd_CANCEL:
	/*
	 * Put the status line back as it should be.
	 */
	show_file_info(curwin, TRUE);
	retval = TRUE;
	break;

    case cmd_INCOMPLETE:
	retval = FALSE;
	break;

    case cmd_COMPLETE:
	cmdline = get_cmd(curwin);
	savedline = strsave(cmdline);
	switch (cmdline[0]) {
	case '/':
	case '?':
	    (void) xvProcessSearch(cmdline[0], cmdline + 1);
	    break;

	case '!':
	    do_pipe(curwin, cmdline + 1);
	    break;

	case ':':
	    exCommand(cmdline + 1, TRUE);
	}
	if (savedline != NULL) {
	    (void) yank_str(savedline[0], savedline, TRUE);
	    free(savedline);
	}
    }
    return(retval);
}

/*ARGSUSED*/
static bool_t
d_proc(c)
int	c;
{
    if (c == CTRL('C')) {
	/*
	 * In some environments it's possible to type
	 * control-C without actually generating an interrupt,
	 * but if they do, in this context, they probably want
	 * the semantics of an interrupt anyway.
	 */
	imessage = (kbdintr = 1);
    }
    return(disp_screen(curwin, c));
}
