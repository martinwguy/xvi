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

/*
 * Should commands be chatty on the status line?
 * True if reading commands from tty.
 * It starts out FALSE so that the calls in xvi_startup are quiet.
 */
bool_t		interactive = FALSE;

xvResponse *
xvi_handle_event(ev)
xvEvent	*ev;
{
    static xvResponse	resp;
    bool_t		do_update;
    int			c;

    /*
     * POSIX requires us to exit non-zero on SIGHUP and SIGTERM but 0 for
     * commands that quit.
     * We see the signals here as Ev_ events, but the commands that exit set
     * State = EXITING elsewhere, so we set it to 0 here and to 1 on signals.
     */

    resp.xvr_status = 0;

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
	/*redraw_all(ev->ev_do_clear);*/
	redraw_all(TRUE);
	break;

    case Ev_resize:
	{
	    if (!vs_resize(ev->ev_vs, ev->ev_rows, ev->ev_columns)) {
		/*
		 * Oh shit. Can't allocate space for new window size.
		 * This is very difficult to fix...
		 */
		(void) exPreserveAllBuffers();
		show_error(out_of_memory);
	    }
	    xvAdjustWindows(ev->ev_vs);
	    move_cursor_to_window();
	    cursupdate();
	    wind_goto();	/* ensure cursor is where it should be */
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
	/*
	 * We don't have to handle this; any code
	 * which is actually interruptible will check the
	 * kbdintr variable itself at appropriate points.
	 * This code only gets executed when we get an
	 * interrupt in a non-interruptible code path, or
	 * when we aren't doing anything at all.
	 */
	break;

    case Ev_suspend_request:
	/*
	 * We only allow editor suspension in top-level command state.
	 */
	if (State == NORMAL) {
	    exSuspend(FALSE);
	} else if (State == SUBNORMAL) {
	    /*
	     * Treat ^Z as 2nd char of a two-char command as being an ESCAPE.
	     * This will cause a beep; a subsequent ^Z may then be issued to
	     * suspend the editor.
	     */
	    map_char(ESC);
	} else {
	    beep();
	}
	break;

    case Ev_terminate:
    case Ev_disconnected:
	/*
	 * Preserve modified buffers, and then exit.
	 */
	(void) exPreserveAllBuffers();
	resp.xvr_status = 1;
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
	if (State != EXITING && (*func)(c)) {
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
	case INSERT:
	case REPLACE:
	    if (Pb(P_showmode)) {
		update_sline();
	    }
	    /* Drop through... */
	case SUBNORMAL:
	    if (do_update) {
		move_window_to_cursor();
		cursupdate();
	    }
	    wind_goto();
	    break;

	case EXITING:
	    break;
	}
    }

    if (State == EXITING) {
	resp.xvr_type = Xvr_exit;
	return(&resp);
    }

    if (imessage) {
	show_message("Interrupted");
	wind_goto();	/* put cursor back */
	imessage = FALSE;
    }

    VSflush(ev->ev_vs);

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
 * Process the given character in normal (command) mode.
 *
 * Return TRUE if screen wants updating, FALSE otherwise.
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
 * Process the given character in command-line mode.
 *
 * Returns TRUE if screen wants updating, FALSE otherwise.
 */
static bool_t
c_proc(c)
int	c;
{
    char	*cmdline;
    char	*savedline;
    bool_t	retval = TRUE;

    switch (cmd_input(c)) {
    case cmd_CANCEL:
	/*
	 * Put the status line back as it should be.
	 */
	info_update();
	break;

    case cmd_INCOMPLETE:
	retval = FALSE;
	break;

    case cmd_COMPLETE:
	cmdline = get_cmd();
	savedline = strsave(cmdline);
	switch (cmdline[0]) {
	case '/':
	case '?':
	    if (!xvProcessSearch(cmdline[0], cmdline + 1)) {
		unstuff();
	    }
	    break;

	case '!':
	    if (!do_pipe(cmdline + 1)) {
		unstuff();
	    }
	    break;

	case ':':
	    interactive = TRUE;
	    if (!exCommand(cmdline + 1)) {
		unstuff();
	    }
	    break;
	}
	if (savedline != NULL) {
	    (void) yank_str(savedline[0], savedline, TRUE);
	    free(savedline);
	}
    }
    return(retval);
}

/*
 * Process a keypress in display mode (printing lines or showing parameters)
 *
 * Return TRUE if screen wants updating, FALSE otherwise.
 */
/*ARGSUSED*/
static bool_t
d_proc(c)
int	c;
{
    return(disp_screen(c));
}
