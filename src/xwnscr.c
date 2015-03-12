/* Copyright (c) 1992 Mark Russell, University of Kent */
/* Modifications Copyright (c) 1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    PD version of UNIX "vi" editor, with extensions.
* module name:
    xwnscr.c
* module function:
    VirtScr implementation over Wn and X11
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include <wn.h>
#include "xvi.h"

typedef struct {
    int		xpos;		/* Cursor column (in characters) */
    int		ypos;		/* Cursor row (in characters) */
    font_t	*font;		/* Text font */
    int		wn;		/* Wn window number for xwn window */
    int		fg;		/* Foreground colour (see xw_set_colour) */
    int		bg;		/* Background colour */
    int		cwidth;		/* Character width in pixels */
    int		cheight;	/* Character height in pixels */
    bool_t	selected;	/* Is the cursor in the xvi window? */
    char	*line_text;	/* Current line buffer (see render_line) */
    int		line_xpos;	/* Column to render output line */
    int		line_ypos;	/* Row to render output line (or -1) */
    int		do_redraw;	/* If TRUE, remember to ask for a redraw */
} wininfo_t;

/*  We set line_ypos to -1 to indicate we have no current output line.
 *  These macros avoid spreading knowledge of that convention through the code.
 */
#define have_line(wi)	((wi)->line_ypos != -1)
#define forget_line(wi)	((wi)->line_ypos = -1)

#define GETWI(vs)	((wininfo_t *)(vs)->pv_sys_ptr)

#define WIN_MARGIN	2

static	void		flip_cursor P((wininfo_t *));
static	bool_t		handle_mouse_move P((VirtScr *, wininfo_t *, event_t *));
static	wininfo_t	*xw_open_window P((int *p_cols, int *p_rows));
static	void		get_input_event P((xvEvent *, VirtScr *, long));
static	void		render_line P((wininfo_t *wi));

static	VirtScr		*xw_new P((VirtScr *));
static	void		xw_close P((VirtScr *));
static	void		xw_clear_all P((VirtScr *));
static	void		xw_clear_rows P((VirtScr *, int, int));
static	void		xw_clear_line P((VirtScr *, int, int));
static	void		xw_goto P((VirtScr *, int, int));
static	void		xw_advise P((VirtScr *, int, int, int, char *));
static	void		xw_putc P((VirtScr *, int, int, int));
static	void		xw_write P((VirtScr *, int, int, char *));
/* static void		xw_insert P((VirtScr *, int, int, char *)); */
static	void		xw_set_colour P((VirtScr *, int));
static	int		xw_colour_cost P((VirtScr *));
static	int		xw_scroll P((VirtScr *, int, int, int));
static	void		xw_flush P((VirtScr *));
static	void		xw_beep P((VirtScr *));

static	bool_t		Use_tty;

VirtScr	xwnscr = {
    NULL,		/* pv_sys_ptr       */
    0,			/* pv_rows	    */
    0,			/* pv_cols	    */
    NULL,		/* pv_window	    */
    NULL,		/* pv_int_lines     */
    NULL,		/* pv_ext_lines     */
    { 0, },		/* pv_colours       */

    xw_new,		/* v_open	    */
    xw_close,		/* v_close	    */
    xw_clear_all,	/* v_clear_all	    */
    xw_clear_rows,	/* v_clear_rows	    */
    xw_clear_line,	/* v_clear_line	    */
    xw_goto,		/* v_goto	    */
    xw_advise,		/* v_advise	    */
    xw_write,		/* v_write	    */
    xw_putc,		/* v_putc	    */
    xw_set_colour,	/* v_set_colour	    */
    xw_colour_cost,	/* v_colour_cost    */
    xv_decode_colour,	/* v_decode_colour  */
    xw_flush,		/* v_flush	    */
    xw_beep,		/* v_beep	    */

    NOFUNC,		/* v_insert	    */
    xw_scroll,		/* v_scroll	    */
    NOFUNC,		/* v_flash	    */
    NOFUNC,		/* v_can_scroll	    */
};

static VirtScr *
xw_new(vs)
VirtScr	*vs;
{
    return NULL;
}

static void
xw_close(vs)
VirtScr	*vs;
{
    wn_close_window(GETWI(vs)->wn);
}

/*  Erase the entire current line.
 */
static void
xw_clear_line(vs, ypos, xpos)
VirtScr *vs;
int ypos, xpos;
{
    wininfo_t *wi;

    wi = GETWI(vs);

    if (have_line(wi))
	render_line(wi);

    wn_set_area(wi->wn,
		xpos * wi->cwidth, ypos * wi->cheight,
		(vs->pv_cols - xpos) * wi->cwidth, wi->cheight,
		WN_BG);

    wi->xpos = xpos;
    wi->ypos = ypos;
}

/*  Erase all the rows from start to end inclusive.
 */
static void
xw_clear_rows(vs, start, end)
VirtScr *vs;
int	start;
int	end;
{
    wininfo_t *wi;

    wi = GETWI(vs);

    if (have_line(wi))
	render_line(wi);

    wn_set_area(wi->wn,
		0, start * wi->cheight,
		vs->pv_cols * wi->cwidth, (end - start + 1) * wi->cheight,
		WN_BG);

    xw_flush(vs);
}

/*  Erase display (may optionally home cursor).
 */
static void
xw_clear_all(vs)
VirtScr *vs;
{
    wininfo_t *wi;

    wi = GETWI(vs);

    forget_line(wi);	/* no point showing it just before clear */

    wn_set_area(wi->wn,
		0, 0,
		vs->pv_cols * wi->cwidth, vs->pv_rows * wi->cheight,
		WN_BG);

    xw_flush(vs);
}

/*  Goto the specified location.
 */
static void
xw_goto(vs, row, col)
VirtScr *vs;
int row, col;
{
    wininfo_t *wi;

    wi = GETWI(vs);

    if (have_line(wi))
	render_line(wi);

    wi->ypos = row;
    wi->xpos = col;
}

/*  TODO: should maybe copy the text into the output line buffer here.
 */
static void
xw_advise(vs, row, col, index, str)
VirtScr *vs;
int row, col, index;
char *str;
{
}

/*  Put out a "normal" character, updating the cursor position.
 *
 *  Xvi expects this call to be cheap, so we accumulate characters
 *  in an output buffer, then render the whole buffer in one X request.
 *  We have to render the line if we see any output operation other
 *  than adding a character or string (via xw_write) at the current
 *  cursor position.
 */
static void
xw_putc(vs, ypos, xpos, c)
VirtScr *vs;
int ypos, xpos, c;
{
    wininfo_t *wi;

    wi = GETWI(vs);

    if (!have_line(wi) || xpos != wi->xpos || ypos != wi->ypos) {
	if (have_line(wi))
	    render_line(wi);
	wi->line_xpos = xpos;
	wi->line_ypos = ypos;
    }

    /*  No NUL termination - see render_line().
     */
    wi->line_text[xpos - wi->line_xpos] = c;
    xpos++;

    wi->xpos = xpos;
    wi->ypos = ypos;
}

/*  Display a buffered line (see xw_putc).
 *
 *  Note that the buffer is not NUL terminated - wn_twrite takes a count.
 */
static void
render_line(wi)
wininfo_t *wi;
{
    wn_twrite(wi->wn, wi->line_text, wi->xpos - wi->line_xpos,
	      wi->line_xpos * wi->cwidth, wi->line_ypos * wi->cheight,
	      wi->fg, wi->bg);

    forget_line(wi);
}

/*  Put out a "normal" string, updating the cursor position.
 *
 *  Handled just like xw_putc, except that for the multiple characters.
 */
static void
xw_write(vs, ypos, xpos, s)
VirtScr *vs;
int ypos, xpos;
register char *s;
{
    wininfo_t *wi;
    int len;

    wi = GETWI(vs);

    if (!have_line(wi) || xpos != wi->xpos || ypos != wi->ypos) {
	if (have_line(wi))
	    render_line(wi);
	wi->line_xpos = xpos;
	wi->line_ypos = ypos;
    }

    /*  No NUL termination - see render_line().
     */
    len = strlen(s);
    (void) memcpy(wi->line_text + (xpos - wi->line_xpos), s, len);

    xpos += len;

    wi->xpos = xpos;
    wi->ypos = ypos;
}

static int
xw_colour_cost(vs)
VirtScr *vs;
{
    return 0;
}

/*  Set the specified colour.  We just support inverse video for the
 *  status line.
 */
static void
xw_set_colour(vs, colour)
VirtScr *vs;
int colour;
{
    wininfo_t *wi;

    wi = GETWI(vs);

    /*  render_line() assumes the current output colour, so we need
     *  to flush on a change of colour.  TODO: remember previous
     *  colour and return on a no-op call.
     */
    if (have_line(wi))
	render_line(wi);

    /*  Copied from the termcap implementation.
     */
    if (colour == 1)
	colour = 0;

    if (colour != 0) {
	wi->fg = WN_BG;
	wi->bg = WN_FG;
    } else {
	wi->fg = WN_FG;
	wi->bg = WN_BG;
    }
}

/*  Scroll an area of the screen.
 */
static int
xw_scroll(vs, start_row, end_row, nlines)
VirtScr *vs;
int start_row, end_row, nlines;
{
    wininfo_t *wi;
    int oldstart, linecount, newstart;

    wi = GETWI(vs);

    if (have_line(wi))
	render_line(wi);

    ++end_row;		/* [start,end] -> [start,end) */

    if (nlines > 0) {
	oldstart = start_row + nlines;
	linecount = end_row - (start_row + nlines);
	newstart = start_row;
    } else {
	oldstart = start_row;
	linecount = end_row - (start_row - nlines);
	newstart = start_row - nlines;
    }

    /*  wn_move_area() handles the clearing of the vacated area.
     */
    wn_move_area(wi->wn,
		 0, oldstart * wi->cheight,
		 vs->pv_cols * wi->cwidth, linecount * wi->cheight,
		 0, newstart * wi->cheight,
		 WN_BG);

    /*  Can't return failure if wn_move_area() failed because I think
     *  the code then assumes that no changes have been made.
     */
    if (wn_last_rop_was_damaged(wi->wn)) {
	wi->do_redraw = TRUE;
    }

    return 1;
}

/*  Flush any pending output, including cursor position.
 */
static void
xw_flush(vs)
VirtScr	*vs;
{
    wininfo_t *wi;

    wi = GETWI(vs);

    if (have_line(wi))
	render_line(wi);

    /*  Hack hack: turn the cursor on, flush updates and turn it off
     *  again immediately, relying on X buffering to keep the cursor
     *  visible on the screen until the next flush.  This is simple
     *  but flaky.  TODO: do it right.
     */
    flip_cursor(wi);
    wn_show_updates(wi->wn);
    flip_cursor(wi);
}

/*  Beep at the user.
 */
static void
xw_beep(vs)
VirtScr *vs;
{
    wn_bell(GETWI(vs)->wn);
    xw_flush(vs);
}

#define EVMASK  (EV_BUTTON_DOWN | EV_MOUSE_MOVED | EV_KEY | \
		 EV_WINDOW_EXPOSED | EV_WINDOW_RESIZED | \
		 EV_WINDOW_SELECTED | EV_WINDOW_DESELECTED | \
		 EV_INTERRUPT)

/*  Get the next input character.  Also handle mouse events and window
 *  exposes on the side.  The mouse handling is not very pretty, but
 *  not all of that is my fault.
 */
static void
get_input_event(xvevent, vs, timeout)
xvEvent		*xvevent;
VirtScr		*vs;
long		timeout;
{
    static int pending_ypos = -1;	/* see below */
    event_t event;
    int last_ypos;
    wininfo_t *wi;

    wi = GETWI(vs);

    if (wi->do_redraw) {
	/*
	 * We did something that buggered up the window.
	 * Clear it and redraw it from scratch.
	 */
	xvevent->ev_type = Ev_refresh;
	xvevent->ev_do_clear = TRUE;
	wi->do_redraw = FALSE;
	return;
    }

    /*  `Should not happen' - we would expect a VSflush() before
     *  reading input.  But be paranoid anyway.
     */
    if (have_line(wi))
	render_line(wi);

    last_ypos = -1;

    /*  If the use moves the mouse outside the current buffer while
     *  in insert mode, we don't break out.  Instead we note that
     *  there is an update pending by setting pending_ypos.
     *  The next time we are called with State==NORMAL we call
     *  mousemove() to update the current buffer.  The effect is
     *  that the selected window changes when the user hits ESC.
     */
    if (State == NORMAL && pending_ypos != -1) {
	xvevent->ev_type = Ev_mousemove;
	xvevent->ev_m_row = pending_ypos;
	pending_ypos = -1;
	return;
    }

    wn_set_input_timeout(timeout);

    for (;;) {
	int ypos;

	wn_next_event(wi->wn, EVMASK, &event);

	switch (event.ev_type) {
	case EV_INTERRUPT:
	    /*
	     * Either a timeout, or some kind of interrupt.
	     */
	    if (kbdintr) {
		xvevent->ev_type = Ev_breakin;
		kbdintr = FALSE;
	    } else if (SIG_terminate) {
		xvevent->ev_type = Ev_terminate;
		SIG_terminate = FALSE;
	    } else if (SIG_suspend_request) {
		xvevent->ev_type = Ev_suspend_request;
		SIG_suspend_request = FALSE;
	    } else if (SIG_user_disconnected) {
		xvevent->ev_type = Ev_disconnected;
		SIG_user_disconnected = FALSE;
	    } else {
		xvevent->ev_type = Ev_timeout;
	    }
	    return;

	case EV_WINDOW_SELECTED:
	case EV_WINDOW_DESELECTED:
	    /*  Need to switch between hollow and solid cursor.
	     *  xw_flush() calls flip_cursor() and so shows the
	     *  update.
	     */
	    wi->selected = event.ev_type == EV_WINDOW_SELECTED;
	    xw_flush(vs);
	    break;

	case EV_WINDOW_RESIZED:
	    {
		int old_rows, old_cols;
		int width, height;
		char *new_line_text;

		old_rows = vs->pv_rows;
		old_cols = vs->pv_cols;

		wn_get_window_size(WN_STDWIN, &width, &height);

		width -= 2 * WIN_MARGIN;
		height -= 2 * WIN_MARGIN;
                wn_set_win_size(wi->wn, width, height);

		vs->pv_cols = width / wi->cwidth;
		vs->pv_rows = height / wi->cheight;

		new_line_text = malloc(vs->pv_cols);
		if (new_line_text == NULL) {
		    xw_beep(vs);
		    continue;		/* get next event */
		}
		free(wi->line_text);
		wi->line_text = new_line_text;

		xvevent->ev_type = Ev_resize;
		xvevent->ev_rows = vs->pv_rows - old_rows;
		xvevent->ev_columns = vs->pv_cols - old_cols;
	    }
	    return;

	case EV_WINDOW_EXPOSED:
	    xvevent->ev_type = Ev_refresh;
	    xvevent->ev_do_clear = FALSE;
	    return;

	case EV_MOUSE_MOVED:
	    ypos = event.ev_y / wi->cheight;		

	    /*  Only need to consider calling mousemove()
	     *  if the mouse has moved to a different line.
	     */
	    if (ypos != last_ypos) {
		if (State == NORMAL) {
		    xvevent->ev_type = Ev_mousemove;
		    xvevent->ev_m_row = ypos;
		    last_ypos = ypos;
		    return;
		} else {
		    pending_ypos = ypos;
		}
	    }
	    break;

	case EV_KEY:
	    xvevent->ev_type = Ev_char;
	    xvevent->ev_inchar = event.ev_char;
	    return;

	case EV_BUTTON_DOWN:
	    /*  If the user clicks in another buffer while in
	     *  insert mode we fake an ESC and push back the
	     *  event.  We will process the event again on the
	     *  next call and do the mouse move handling.
	     *
	     *  TODO: do this stuff right.
	     */
	    if (State != NORMAL) {
		wn_pushback_event(&event);
		xvevent->ev_type = Ev_char;
		xvevent->ev_inchar = ESC;
		break;
	    }

	    /*  If they didn't move the mouse it was a mouse click on a single
	     *  character.
	     */
	    if (!handle_mouse_move(vs, wi, &event)) {
		xvevent->ev_type = Ev_mouseclick;
		xvevent->ev_m_row = event.ev_y / wi->cheight;
		xvevent->ev_m_col = event.ev_x / wi->cwidth;
		return;
	    }
	    break;
	}

	/*  If a mouse button has been pressed we may have done
	 *  a drag or something, so the current position of the
	 *  mouse is unknown.  Force a check at the next
	 *  EV_MOUSE_MOVED event.
	 */
	if (event.ev_type != EV_MOUSE_MOVED)
	    last_ypos = -1;
    }
}

/*  Handle the mouse being moved around with the button pressed.
 */
static bool_t
handle_mouse_move(vs, wi, ev)
VirtScr *vs;
wininfo_t *wi;
event_t *ev;
{
    int xpos, ypos, old_xpos, old_ypos;
    bool_t done_drag;
    static xvEvent mouse_event = {
    	Ev_mousedrag,
    };

    old_xpos = ev->ev_x / wi->cwidth;
    old_ypos = ev->ev_y / wi->cheight;

    done_drag = FALSE;

    mouse_event.ev_vs = vs;
    for (;;) {
	wn_next_event(wi->wn, EV_BUTTON_UP | EV_MOUSE_MOVED, ev);

	xpos = ev->ev_x / wi->cwidth;
	ypos = ev->ev_y / wi->cheight;

	if (ev->ev_type == EV_BUTTON_UP)
		break;

	/*  We call mousedrag() every time the cursor position
	 *  changes.  This means that the buffer dividing lines
	 *  track the mouse, giving the use feedback on what is
	 *  going to happen.
	 */
	if (xpos != old_xpos || ypos != old_ypos) {
	    xvResponse	*resp;

	    mouse_event.ev_m_row = old_ypos;
	    mouse_event.ev_m_col = old_xpos;
	    mouse_event.ev_m_endrow = ypos;
	    mouse_event.ev_m_endcol = xpos;
	    resp = xvi_handle_event(&mouse_event);
	    if (resp->xvr_type == Xvr_exit) {
	    	exit(0);
	    }

	    old_xpos = xpos;
	    old_ypos = ypos;
	    done_drag = TRUE;
	}
    }
    return(done_drag);
}

/*  Open a window and get fonts.
 */
static wininfo_t *
xw_open_window(p_cols, p_rows)
int *p_cols, *p_rows;
{
    int wn, win_width, win_height;
    wininfo_t *wi;

    if ((wi = (wininfo_t *)malloc(sizeof(wininfo_t))) == NULL) {
	/*  TODO: is this the right way to exit - probably not.
	 */
	fprintf(stderr, "Can't allocate memory (%s)", strerror(errno));
	exit(1);
    }

    wi->font = wn_get_sysfont();
    wi->fg = WN_FG;
    wi->bg = WN_BG;
    wi->cwidth = wi->font->ft_width;
    wi->cheight = wi->font->ft_height;
    wi->selected = FALSE;
    wi->do_redraw = FALSE;

    wn_suggest_resize_inc(WIN_MARGIN * 2, WIN_MARGIN * 2,
			  wi->cwidth, wi->cheight);
    wn_suggest_window_size(80, 24);

    if ((wn = wn_open_stdwin()) == -1) {
	startup_error("Can't open window\n");
	exit(1);
    }

    wn_get_window_size(wn, &win_width, &win_height);
    wi->wn = wn_create_subwin(wn, WIN_MARGIN,
    				  WIN_MARGIN,
				  win_width - 2 * WIN_MARGIN,
				  win_height - 2 * WIN_MARGIN,
				  WN_INPUT_OUTPUT);

    /*  We do explicit output flushes in xw_flush().
     */
    wn_updating_off(wn);

    *p_cols = win_width / wi->cwidth;
    *p_rows = win_height / wi->cheight;

    /*  The buffer is not NUL terminated, so no need for the usual +1.
     */
    if ((wi->line_text = malloc(*p_cols)) == NULL) {
	fprintf(stderr, "Can't allocate memory (%s)", strerror(errno));
	exit(1);
    }
    forget_line(wi);	/* no valid contents yet */

    return wi;
}

static void
flip_cursor(wi)
wininfo_t *wi;
{
    int x, y;

    x = wi->xpos * wi->cwidth;
    y = wi->ypos * wi->cheight;

    /*  Hollow cursor if the mouse is in our window, solid otherwise.
     */
    if (wi->selected)
	wn_invert_area(wi->wn, x, y, wi->cwidth, wi->cheight);
    else
	wn_invert_box(wi->wn, x, y, wi->cwidth, wi->cheight);
}

int
main(argc, argv)
int argc;
char **argv;
{
    VirtScr	*vs;
    xvEvent	event;
    const char	*mesg;

    /*  If DISPLAY is set we use the X interface, otherwise we fall
     *  back to the tty interface.  This means I can use the same
     *  binary for work and for logging in from home.
     */
    Use_tty = (getenv("DISPLAY") == NULL);

    if (Use_tty) {
	tcap_scr_main(argc, argv);
	exit(0);
    }

    if ((mesg = wn_open_display()) != NULL) {
        startup_error(mesg);
        startup_error("\n");
        exit(1);
    }

    argc = wn_munge_args(argc, (const char **)argv);

    vs = &xwnscr;
    vs->pv_sys_ptr = (genptr *) xw_open_window(&vs->pv_cols, &vs->pv_rows);

    subshells = FALSE;	/* make sure we don't attempt to shell out */

    ignore_signals();
    if (xvi_startup(vs, argc, argv, getenv("XVINIT")) == NULL) {
        exit(1);
    }
    catch_signals();

#ifdef SIGTSTP
    /*  No special treatment of SIGTSTP if we are running in a window.
     */
    signal(SIGTSTP, SIG_DFL);
#endif
	
    event.ev_vs = vs;
    for (;;) {
	long		timeout = 0;
	xvResponse	*resp;

	get_input_event(&event, vs, timeout);

	resp = xvi_handle_event(&event);
	if (resp->xvr_type == Xvr_exit) {
	    break;
	}
	timeout = resp->xvr_timeout;
    }
    exit(0);
}

void
startup_error(str)
char	*str;
{
    static int	called = 0;

    if (!called) {
	if (Use_tty) {
	    sys_endv();
	}
	called = 1;
    }
    (void) fputs(str, stderr);
}
