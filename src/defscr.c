/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    defscr.c
* module function:
    VirtScr interface using old style porting functions.
    We assume newscr() is only called once; it is an
    error for it to be called more than once.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

static	VirtScr		*newscr P((VirtScr *, genptr *));
static	void		closescr P((VirtScr *));
static	void		clear_all P((VirtScr *));
static	void		clear_rows P((VirtScr *, int, int));
static	void		clear_line P((VirtScr *, int, int));
static	void		xygoto P((VirtScr *, int, int));
static	void		xyadvise P((VirtScr *, int, int, int, char *));
static	void		put_str P((VirtScr *, int, int, char *));
static	void		put_char P((VirtScr *, int, int, int));
static	void		ins_str P((VirtScr *, int, int, char *));
static	void		pset_colour P((VirtScr *, int));
static	int		colour_cost P((VirtScr *));
static	int		defscroll P((VirtScr *, int, int, int, bool_t));
static	int		scroll P((VirtScr *, int, int, int));
static	int		can_scroll P((VirtScr *, int, int, int));
static	void		flushout P((VirtScr *));
static	void		pbeep P((VirtScr *));

VirtScr	defscr = {
    NULL,		/* pv_sys_ptr       */
    0,			/* pv_rows	    */
    0,			/* pv_cols	    */
    NULL,		/* pv_window	    */
    NULL,		/* pv_int_lines     */
    NULL,		/* pv_ext_lines     */
    { 0, },		/* pv_colours       */

    newscr,		/* v_open	    */
    closescr,		/* v_close	    */
    clear_all,		/* v_clear_all	    */
    clear_rows,		/* v_clear_rows	    */
    clear_line,		/* v_clear_line	    */
    xygoto,		/* v_goto	    */
    xyadvise,		/* v_advise	    */
    put_str,		/* v_write	    */
    put_char,		/* v_putc	    */
    pset_colour,	/* v_set_colour	    */
    colour_cost,	/* v_colour_cost    */
    xv_decode_colour,	/* v_decode_colour  */
    flushout,		/* v_flush	    */
    pbeep,		/* v_beep	    */

    ins_str,		/* v_insert	    */
    scroll,		/* v_scroll	    */
    NOFUNC,		/* v_flash	    */
    can_scroll,		/* v_can_scroll     */
};

void
defscr_main(argc, argv)
int	argc;
char	*argv[];
{
    xvEvent	event;
    long	timeout = 0;

    /*
     * Set up the system and terminal interfaces. This establishes
     * the window size, changes to raw mode and does whatever else
     * is needed for the system we're running on.
     */
    sys_init();

    if (!can_inschar) {
	defscr.v_insert = NOFUNC;
    }
    if (!can_scroll_area && !can_ins_line && !can_del_line) {
	defscr.v_scroll = NOFUNC;
    }
    defscr.pv_rows = (unsigned) Rows;
    defscr.pv_cols = (unsigned) Columns;

    ignore_signals();
    if (xvi_startup(&defscr, argc, argv, getenv("XVINIT")) == NULL) {
	sys_endv();
	exit(1);
    }
    catch_signals();

    event.ev_vs = &defscr;
    while (1) {
	xvResponse	*resp;
	register int	r;

	r = inchar(timeout);
	if (r == EOF) {
	    if (kbdintr) {
		event.ev_type = Ev_breakin;
		kbdintr = FALSE;
	    } else if (SIG_terminate) {
		event.ev_type = Ev_terminate;
		SIG_terminate = FALSE;
	    } else if (SIG_suspend_request) {
		event.ev_type = Ev_suspend_request;
		SIG_suspend_request = FALSE;
	    } else if (SIG_user_disconnected) {
		event.ev_type = Ev_disconnected;
		SIG_user_disconnected = FALSE;
	    } else {
		event.ev_type = Ev_timeout;
	    }
	} else {
	    event.ev_type = Ev_char;
	    event.ev_inchar = r;
	}
	resp = xvi_handle_event(&event);
	if (resp->xvr_type == Xvr_exit) {
	    sys_exit(0);
	}
	timeout = resp->xvr_timeout;
    }
}

/*ARGSUSED*/
static VirtScr *
newscr(scr, win)
VirtScr	*scr;
genptr	*win;
{
    return(NULL);
}

/*ARGSUSED*/
static void
closescr(scr)
VirtScr	*scr;
{
}

/*ARGSUSED*/
static void
clear_all(scr)
VirtScr	*scr;
{
    erase_display();
}

/*ARGSUSED*/
static void
clear_rows(scr, start, end)
VirtScr	*scr;
int	start;
int	end;
{
    if (start == 0 && end == (scr->pv_rows - 1)) {
	erase_display();
    } else {
    	int	row;

	for (row = start; row <= end; row++) {
	    tty_goto(row, 0);
	    erase_line();
	}
    }
}

/*ARGSUSED*/
static void
clear_line(scr, row, col)
VirtScr	*scr;
int	row;
int	col;
{
    tty_goto(row, col);
    if (col < scr->pv_cols) {
	erase_line();
    }
}

/*ARGSUSED*/
static void
xygoto(scr, row, col)
VirtScr	*scr;
int	row;
int	col;
{
    tty_goto(row, col);
}

/*ARGSUSED*/
static void
xyadvise(scr, row, col, index, str)
VirtScr	*scr;
int	row;
int	col;
int	index;
char	*str;
{
    if (index > cost_goto) {
	tty_goto(row, col + index);
    } else {
	tty_goto(row, col);
	while (index-- > 0) {
	    outchar(*str++);
	}
    }
}

/*ARGSUSED*/
static void
put_str(scr, row, col, str)
VirtScr	*scr;
int	row;
int	col;
char	*str;
{
    tty_goto(row, col);
    outstr(str);
}

/*ARGSUSED*/
static void
put_char(scr, row, col, c)
VirtScr	*scr;
int	row;
int	col;
int	c;
{
    tty_goto(row, col);
    outchar(c);
}

/*ARGSUSED*/
static void
ins_str(scr, row, col, str)
VirtScr	*scr;
int	row;
int	col;
char	*str;
{
    /*
     * If we are called, can_inschar is TRUE,
     * so we know it is safe to use inschar().
     */
    tty_goto(row, col);
    for ( ; *str != '\0'; str++) {
	inschar(*str);
    }
}

/*ARGSUSED*/
static void
pset_colour(scr, colour)
VirtScr	*scr;
int	colour;
{
    set_colour(colour);
}

/*ARGSUSED*/
static int
colour_cost(scr)
VirtScr	*scr;
{
#ifdef	SLINE_GLITCH
    return(SLINE_GLITCH);
#else
    return(0);
#endif
}

/*ARGSUSED*/
static int
defscroll(scr, start_row, end_row, nlines, doit)
VirtScr	*scr;
int	start_row;
int	end_row;
int	nlines;
bool_t	doit;
{
    if (nlines < 0) {
	/*
	 * nlines negative means scroll reverse - i.e. move
	 * the text downwards with respect to the terminal.
	 */
	nlines = -nlines;

	if (can_scroll_area) {
	    if (doit) {
		scroll_down(start_row, end_row, nlines);
	    }
	} else if (can_ins_line && end_row == Rows - 1) {
	    if (doit) {
		int	line;

		for (line = 0; line < nlines; line++) {
		    tty_goto(start_row, 0);
		    insert_a_line();
		}
	    }
	} else {
	    return(0);
	}
    } else if (nlines > 0) {
	/*
	 * Whereas nlines positive is "normal" scrolling.
	 */
	if (can_scroll_area) {
	    if (doit) {
		scroll_up(start_row, end_row, nlines);
	    }
	} else if (end_row == Rows - 1) {
	    if (can_del_line) {
		if (doit) {
		    int	line;

		    for (line = 0; line < nlines; line++) {
			tty_goto(start_row, 0);
			delete_a_line();
		    }
		}
	    } else {
		return(0);
	    }
	} else {
	    return(0);
	}
    }
    return(1);
}

static int
scroll(scr, start_row, end_row, nlines)
VirtScr	*scr;
int	start_row;
int	end_row;
int	nlines;
{
    return defscroll(scr, start_row, end_row, nlines, TRUE);
}

/*
 * This function has exactly the same return value as scroll() above,
 * but it doesn't do the scrolling - it just returns true or false.
 */
/*ARGSUSED*/
static int
can_scroll(scr, start_row, end_row, nlines)
VirtScr	*scr;
int	start_row;
int	end_row;
int	nlines;
{
    return defscroll(scr, start_row, end_row, nlines, FALSE);
}

/*ARGSUSED*/
static void
flushout(scr)
VirtScr	*scr;
{
    flush_output();
}

/*ARGSUSED*/
static void
pbeep(scr)
VirtScr	*scr;
{
    alert();
    flush_output();
}
