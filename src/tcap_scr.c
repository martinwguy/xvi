/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    tcap_scr.c
* module function:
    VirtScr interface for termcap under UNIX.

    The following capabilities are not yet used, but may be added in future
    to improve efficiency (at the expense of code compactness):

	cr	str	Carriage return, (default ^M)
	dC	num	Number of milliseconds of cr delay needed
	nc	bool	No correctly working carriage return (DM2500,H2000)
	xr	bool	Return acts like ce \r \n (Delta Data)

	ch	str	Like cm but horizontal motion only, line stays same
	cv	str	Like cm but vertical motion only, column stays same
	DO	str	down N lines
	UP	str	up N lines
	LE	str	left N chars
	RI	str	right N spaces

	ll	str	Last line, first column
	sc	str	save cursor
	rc	str	restore cursor from last "sc"
	dB	num	Number of milliseconds of bs delay needed

	AL	str	add N new blank lines
	DL	str	delete N lines

	dc	str	Delete character
	dm	str	Delete mode (enter)
	ed	str	End delete mode
	ip	str	Insert pad after character inserted
	in	bool	Insert mode distinguishes nulls on display
	mi	bool	Safe to move while in insert mode

	dF	num	Number of milliseconds of ff delay needed
	cd	str	Clear to end of display

	xs	bool	Standout not erased by writing over it (HP 264?)

* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#ifdef AIX
#include <curses.h>
#else
#include <termcap.h>
#endif

#include "xvi.h"

static	VirtScr		*newscr P((VirtScr *, genptr *));
static	void		closescr P((VirtScr *));
static	void		clear_all P((VirtScr *));
static	void		clear_line P((VirtScr *, int, int));
static	void		xygoto P((VirtScr *, int, int));
static	void		put_str P((VirtScr *, int, int, char *));
static	void		put_char P((VirtScr *, int, int, int));
static	void		ins_str P((VirtScr *, int, int, char *));
static	void		pset_colour P((VirtScr *, int));
static	int		scroll P((VirtScr *, int, int, int));
static	int		can_scroll P((VirtScr *, int, int, int));
static	void		flushout P((VirtScr *));
static	void		pbeep P((VirtScr *));
static	void		do_auto_margin_motion P((void));

VirtScr	tcap_scr = {
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
    clear_line,		/* v_clear_line	    */
    xygoto,		/* v_goto	    */
    put_str,		/* v_write	    */
    put_char,		/* v_putc	    */
    pset_colour,	/* v_set_colour	    */
    xv_decode_colour,	/* v_decode_colour  */
    flushout,		/* v_flush	    */
    pbeep,		/* v_beep	    */

    ins_str,		/* v_insert	    */
    scroll,		/* v_scroll	    */
    can_scroll,		/* v_can_scroll     */
};

static	void	xyupdate P((void));
static	void	fail P((char *));
static	void	set_scroll_region P((int, int));

/*
 * These are used to optimise output - they hold the current
 * "real" and "virtual" coordinates of the cursor, i.e. where
 * it is and where it is supposed to be.
 *
 * The "optimise" variable says whether we should cache cursor
 * positions or should just go to the place asked; it is
 * unset at the start so that the first goto sets things up.
 *
 * Note that the functions "outchar" and "outstr" should be
 * used for strings of ordinary characters; stdio primitives
 * are used internally to put out escape sequences.
 */
static	int	real_row = 0, real_col = 0;
static	int	virt_row = 0, virt_col = 0;
static	bool_t	optimise = FALSE;

/*
 * Termcap-related declarations.
 */
extern	char	*tgetstr();
extern	char	*tgoto();

/*
 * Exported.
 */
static int	cost_home;		/* cost of using the "ho" capability */
static int	cost_cr;		/* cost of using the "cr" capability */
static int	cost_bc;		/* cost of using the "bc" capability */
static int	cost_down;		/* cost of using the "do" capability */
static int	cost_up;		/* cost of using the "up" capability */
bool_t		can_scroll_area = FALSE; /* true if we can set scroll region */
unsigned int	CO = 0;			/* screen dimensions; 0 at start */
unsigned int	LI = 0;

/*
 * Needed by termcap library.
 */
#ifndef AIX
extern	char	PC;	/* pad character, not used by xvi. */
extern	char *	BC;	/* backspace if not ^H. Don't use this. Use bc. */
extern	char *	UP;	/* move up one line. Don't use this. Use up. */
#endif

/*
 * Internal string, num and boolean defs.
 */
static	char	*KS, *KE;		/* keypad transmit start/end */
static	char	*VS, *VE;		/* visual start/end */
static	char	*TI, *TE;		/* cursor motion start/end */
static	char	*CE, *CL;		/* erase line/display */
static	char	*AL, *DL;		/* insert/delete line */
static	char	*IC, *IM, *EI;		/* insert character / insert mode */
static	char	*CM;			/* cursor motion string */
static	char	*HO;			/* cursor to home position */
static	char	*CS;			/* change scroll region */
static	char	*sf, *sr;		/* scroll forward/reverse 1 line */
static	char	*SF, *SR;		/* scroll forward/reverse n lines */
static	char	*MR, *MD, *ME;		/* reverse/double-intensity mode start/end */

static	char	*VB;			/* visual bell */

static	char	*colours[10];		/* colour caps c0 .. c9 */
static	int	ncolours;		/* number of colour caps we have */

static	char	*bc;			/* backspace */
static	char	*nd;			/* non-destructive forward space */
static	char	*up;			/* up one line */
static	char	*down;			/* down one line ("do" is reserved) */
static	char	*cr;			/* carriage return */

static	bool_t	can_move_in_standout;	/* True if can move while SO is on */
static	bool_t	auto_margins;		/* true if AM is set */
static	bool_t	newline_glitch;		/* "xn" capability */

#define	can_backspace	(bc != NULL)
#define	can_fwdspace	(nd != NULL)	/* true if can forward space (nd) */
#define	can_movedown	(down != NULL)	/* true if can move down (do) */
#define	can_moveup	(up != NULL)	/* true if can move up (up) */
#define can_inschar	(IC != NULL || IM != NULL)
#define can_del_line	(DL != NULL)	/* true if we can delete lines */
#define can_ins_line	(AL != NULL)	/* true if we can insert lines */
#define can_clr_to_eol	(CE != NULL)	/* true if can clr-to-eol */

/*
 * A value to return as the cost of a motion that is not available.
 * it should be larger than any possible motion cost, but not INT_MAX
 * because we also want to be able to add these together and still get
 * an unappetising cost as the result.
 * Powers of two produce smaller code on some processors.
 */
#define	NOCM		1024

/*
 * We use this table to perform mappings from cursor keys
 * into appropriate xvi input keys (in command mode).
 */
static unsigned char arrow_keys[] = {
    K_HELP,	'\0',
    K_UNDO,	'\0',
    K_INSERT,	'\0',
    K_HOME,	'\0',
    K_UARROW,	'\0',
    K_DARROW,	'\0',
    K_LARROW,	'\0',
    K_RARROW,	'\0',
    K_CGRAVE,	'\0',
    K_PGDOWN,	'\0',
    K_PGUP,	'\0',
    K_END,	'\0',
    K_DELETE,	'\0',
};
static struct {
    char	*key_tcname;
    char	*key_rhs;
} keys[] = {
  { "%1",	(char *) arrow_keys + 0		}, /* help */
  { "k1",	(char *) arrow_keys + 0		}, /* F1 == help */
  { "&8",	(char *) arrow_keys + 2		}, /* undo */
  { "kI",	(char *) arrow_keys + 4		}, /* insert character */
  { "kh",	(char *) arrow_keys + 6		}, /* home */
  { "ku",	(char *) arrow_keys + 8		}, /* up */
  { "kd",	(char *) arrow_keys + 10	}, /* down */
  { "kl",	(char *) arrow_keys + 12	}, /* left */
  { "kr",	(char *) arrow_keys + 14	}, /* right */
						   /* cgrave ??? */
  { "kN",	(char *) arrow_keys + 18	}, /* page down */
  { "kP",	(char *) arrow_keys + 20	}, /* page up */
  { "@7",	(char *) arrow_keys + 22	}, /* end */
  { "kD",	(char *) arrow_keys + 24	}, /* delete character */
  { "k0",	"#0"				}, /* function key 0 */
  { "k2",	"#2"				}, /* function key 2 */
  { "k3",	"#3"				}, /* function key 3 */
  { "k4",	"#4"				}, /* function key 4 */
  { "k5",	"#5"				}, /* function key 5 */
  { "k6",	"#6"				}, /* function key 6 */
  { "k7",	"#7"				}, /* function key 7 */
  { "k8",	"#8"				}, /* function key 8 */
  { "k9",	"#9"				}, /* function key 9 */
  { "k;",	"#0"				}, /* function key 10 */
  { NULL,	NULL				}
};

/*
 * Used by scroll region optimisation.
 */
static int	s_top = 0, s_bottom = 0;

/*
 * Used for colour-setting optimisation.
 */
#define	NO_COLOUR	-1
static	int	old_colour = NO_COLOUR;	/* Screen's current drawing colour */
static	int	new_colour = NO_COLOUR;	/* The colour they asked for last */

extern	volatile bool_t	win_size_changed;

/*ARGSUSED*/
static void
win_sig_handler(sig)
int	sig;
{
#ifndef _POSIX_SOURCE
    /* Not necessary when using sigaction() instead of signal() */
    (void) signal(SIGWINCH, win_sig_handler);
#endif
    win_size_changed = TRUE;
}

void
tcap_scr_main(argc, argv)
int	argc;
char	*argv[];
{
    register VirtScr	*vs;
    xvEvent		event;
    long		timeout = 0;

    vs = &tcap_scr;

    /*
     * Set up the system and terminal interfaces. This establishes
     * the window size, changes to raw mode and does whatever else
     * is needed for the system we're running on.
     */
    sys_init();

    if (!can_inschar) {
	vs->v_insert = NOFUNC;
    }
    vs->pv_rows = LI;
    vs->pv_cols = CO;

    catch_signals();
    if (xvi_startup(vs, argc, argv) == NULL) {
	sys_endv();
	exit(1);
    }

#ifdef	SV_INTERRUPT
    {
	struct sigvec	vec;

	(void) sigvec(SIGWINCH, (struct sigvec *) NULL, &vec);
	vec.sv_handler = win_sig_handler;
	vec.sv_flags |= SV_INTERRUPT;
	(void) sigvec(SIGWINCH, &vec, (struct sigvec *) NULL);
    }
#else
/* Prefer sigaction() to signal() is available, as it works better */
# ifdef _POSIX_SOURCE
    {
	struct sigaction act;
	act.sa_handler = win_sig_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGWINCH, &act, NULL);
    }
# else
    (void) signal(SIGWINCH, win_sig_handler);
# endif
#endif

    flushout(vs);	/* Flush startup screen */
    event.ev_vs = vs;
    while (1) {
	xvResponse	*resp;
	register int	r;

	r = inch(timeout);
	if (r == EOF) {
	    if (kbdintr) {
		event.ev_type = Ev_breakin;
	    } else if (SIG_terminate) {
		event.ev_type = Ev_terminate;
		SIG_terminate = FALSE;
	    } else if (SIG_suspend_request) {
		event.ev_type = Ev_suspend_request;
		SIG_suspend_request = FALSE;
	    } else if (SIG_user_disconnected) {
		event.ev_type = Ev_disconnected;
		SIG_user_disconnected = FALSE;
	    } else if (win_size_changed) {
		unsigned	new_rows, new_cols;

		win_size_changed = FALSE;

		getScreenSize(&new_rows, &new_cols);
		if (new_rows != 0 && new_cols != 0) {
		    event.ev_type = Ev_resize;
		    event.ev_rows = new_rows - vs->pv_rows;
		    event.ev_columns = new_cols - vs->pv_cols;

		    vs->pv_rows = LI = new_rows;
		    vs->pv_cols = CO = new_cols;
		} else {
		    pbeep(vs);
		    continue;		/* don't process this event */
		}
	    } else {
		event.ev_type = Ev_timeout;
	    }
	} else {
	    event.ev_type = Ev_char;
	    event.ev_inchar = r;
	}
	resp = xvi_handle_event(&event);
	if (resp->xvr_type == Xvr_exit) {
	    sys_exit(resp->xvr_status);
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
clear_line(scr, row, col)
VirtScr	*scr;
int	row;
int	col;
{
    if (col < scr->pv_cols) {
	if (can_clr_to_eol) {
	    tty_goto(row, col);
	    erase_line();
	} else {
	    /*
	     * Output spaces to clear to end of line.
	     *
	     * It's tempting to zero the internal line image and call
	     * xvUpdateLine() but that would be a call to a higher level
	     * in an otherwise strictly downward calling hierarchy.
	     */

	    /* Points to the line on-screen */
	    Sline *ext_line = &scr->pv_ext_lines[row];
	    char *ext;		    /* Points to screen character to clear */
	    unsigned char *colour;  /* and its colour */

	    if (ext_line->s_used <= col) {
		/* That part of the line is unused */
		return;
	    }
	    /* Point to the first screen character to clear */
	    ext    = ext_line->s_line + col;
	    colour = ext_line->s_colour + col;

	    while (*ext) {
		if (*ext != ' ' || *colour != 0) {
		    tty_goto(row, col);
		    xyupdate();
		    moutch(' ');
		    real_col++;
		    if (real_col == CO)
			do_auto_margin_motion();
		    /* Update external line */
		    *ext = ' ';
		    *colour = 0;
		}
		ext++;
		colour++;
		col++;
	    }
	}
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

/*
 * Set the specified colour. Just does standout/standend mode for now.
 * Optimisation here to avoid setting standend when we aren't in
 * standout; assumes calling routines are well-behaved (i.e. only do
 * screen movement in P_colour) or some terminals will write garbage
 * all over the screen.
 */
/*ARGSUSED*/
static void
pset_colour(scr, colour)
VirtScr	*scr;
int	colour;
{
    new_colour = colour;
}

/* Send the escape sequences to set the drawing colour */
static void
do_set_colour(int colour)
{
    if (colour == old_colour) {
	return;
    }

    if (colour < ncolours) {
	/*
	 * Within the range of possible colours.
	 */
	tputs(colours[colour], 1, foutch);
    } else {
	/*
	 * No colour caps, so use standout/standend.
	 * Map colour 2..9 => standout, 0 & 1 => normal.
	 * This is because the default values are:
	 *
	 *	systemcolour	0
	 *	colour		1
	 *	statcolour	2
	 *	roscolour	3
	 */

	if (colour == 1) {
	    colour = 0;
	}

	if (colour == old_colour) {
	    return;
	}

	switch (colour) {
	case 0:				/* no colour */
	    if (ME != NULL) {
		tputs(ME, 1, foutch);
	    }
	    break;

	case 4:				/* double intensity */
	    if (MD != NULL) {
		tputs(MD, 1, foutch);
		break;
	    }
	    /* else FALL THROUGH */

	default:			/* reverse */
	    if (MR != NULL) {
		tputs(MR, 1, foutch);
	    }
	}
    }

    old_colour = colour;
}

/*
 * Scroll the screen up by nlines lines from start_row to end_row inclusive.
 *
 * Negative nlines means scroll reverse - i.e. move the text downwards
 * with respect to the terminal.
 *
 * Return 1 if the scrolling was done or is possible, 0 otherwise.
 *
 * If "doit" is FALSE, don't move the screen and just return whether it's
 * possible or not.
 */
/*ARGSUSED*/
static int
tcscroll(scr, start_row, end_row, nlines, doit)
VirtScr	*scr;
int	start_row;
int	end_row;
int	nlines;
bool_t	doit;
{
    register int	vs_rows;

    vs_rows = scr->pv_rows;

    if (nlines < 0) {
	nlines = -nlines;

	if (can_scroll_area) {
	    if (doit) {
		scroll_down(start_row, end_row, nlines);
	    }
	} else if (can_ins_line && end_row == vs_rows - 1) {
	    if (doit) {
		int	line;

		for (line = 0; line < nlines; line++) {
		    tty_goto(start_row, 0);
		    xyupdate();
		    if (AL != NULL) {
			tputs(AL, vs_rows, foutch);
		    }
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
	} else if (end_row == vs_rows - 1) {
	    int	line;

	    if (start_row == 0) {
		/*
		 * Special case: we want to scroll the whole screen.
		 * The most efficient way to do this is by just going
		 * to the bottom & putting out the right number of
		 * newlines, which is also what vi does in this case.
		 */
		if (doit) {
		    tty_goto(end_row, 0);
		    xyupdate();
		    for (line = 0; line < nlines; line++) {
			tputs(down, 1, foutch);
		    }
		}
	    } else if (can_del_line) {
		if (doit) {
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

/*
 * Virtual functions for Virtscr, returning 1 if successful, 0 if not.
 */
static int
scroll(scr, start_row, end_row, nlines)
VirtScr	*scr;
int	start_row;
int	end_row;
int	nlines;
{
    return tcscroll(scr, start_row, end_row, nlines, TRUE);
}

/*
 * This function has exactly the same return value as scroll() above,
 * but it doesn't do the scrolling - it just returns true or false.
 */
static int
can_scroll(scr, start_row, end_row, nlines)
VirtScr	*scr;
int	start_row;
int	end_row;
int	nlines;
{
    return tcscroll(scr, start_row, end_row, nlines, FALSE);
}

/*ARGSUSED*/
static void
flushout(scr)
VirtScr	*scr;
{
    xyupdate();
    oflush();
}

/*
 * Beep at the user.
 *
 * Use visual bell if it's there and the vbell parameter says to use it.
 */
/*ARGSUSED*/
static void
pbeep(scr)
VirtScr	*scr;
{
    if (Pb(P_vbell) && VB != NULL) {
	xyupdate();
	tputs(VB, (int) scr->pv_rows, foutch);
	optimise = FALSE;
    } else {
	moutch('\007');
    }
    oflush();
}

/*
 * We have just output a character on the last column of the screen.
 * This function figures out where that leaves the cursor.
 *
 * When this is called, real_col should == CO.
 */
static void
do_auto_margin_motion()
{
    if (auto_margins) {
	if (!newline_glitch) {
	    /* Normal wrap */
	    real_col = 0;
	    real_row += 1;
	} else {
	    /*
	     * The glitch would eat a following newline so force
	     * the next motion to be done with a CM string.
	     */
	    optimise = FALSE;
	}
    } else {
	/* !auto_margins means the cursor remains at end of line. */
	real_col--;
    }
}

/**************************************************************************************
 **************************************************************************************
 **************************************************************************************
 **************************************************************************************
 **************************************************************************************/

/*
 * Put out a "normal" character, updating the cursor position.
 */
void
outchar(c)
register int	c;
{
    xyupdate();
    do_set_colour(new_colour);
    real_col++;
    virt_col++;
    moutch(c);
    if (real_col >= CO)
	do_auto_margin_motion();
}

/*
 * Put out a "normal" string, updating the cursor position.
 */
void
outstr(s)
register char	*s;
{
    xyupdate();
    do_set_colour(new_colour);
    while (*s != '\0') {
	real_col++;
	virt_col++;
	moutch(*s++);
    }

    /*
     * We only worry about whether we have hit the right-hand margin
     * at the end of the string; this is okay so long as we can trust
     * the calling code not to use outstr if the string is going to
     * wrap around.
     */
    if (real_col >= CO)
	do_auto_margin_motion();
}

/*
 * This routine is called by tty_open() if for some reason the terminal
 * is unsuitable.
 */
static void
fail(str)
char	*str;
{
    /*
     * Assume we are in raw mode already, so set back into cooked.
     */
    sys_endv();

    (void) fputs(str, stderr);
    putc('\n', stderr);

    exit(2);
}

/* Utility function used as a counting substitute for foutch()
 * when using tputs() just to count the output characters. */
static int cost;

static int
inc_cost(c)
int c;
{
    cost++;
    return(c);
}

/*
 * Look up term entry in termcap database, and set up all the strings.
 */
void
tty_open(prows, pcolumns)
unsigned int	*prows;
unsigned int	*pcolumns;
{
    char	tcbuf[4096];		/* buffer for termcap entry */
    char	*termtype;		/* terminal type */
    static	char strings[512];	/* space for storing strings */
    char	*strp = strings;	/* ptr to space left in strings */
    char	*cp;			/* temp for single char strings */
    int	i;

    termtype = getenv("TERM");
    if (termtype == NULL || *termtype == '\0') {
	termtype = "ansi";
    }
    switch (tgetent(tcbuf, termtype)) {
    case -1:
	fail("Can't open termcap.");
	/*NOTREACHED*/
    case 0:
	fail("Can't find entry for your terminal in termcap.");
	/*NOTREACHED*/
    }

    /*
     * Booleans.
     */
    auto_margins	= (bool_t) tgetflag("am");
    newline_glitch	= (bool_t) tgetflag("xn");
    can_move_in_standout = (bool_t) tgetflag("ms");
    /*
     * Standout glitch: number of spaces left when entering or leaving
     * standout mode. Xvi cannot function on such terminals.
     */
    if (tgetnum("sg") > 0) {
	fail("xvi doesn't work on terminals with the standout glitch.");
    }

    /*
     * Integers.
     */

    /*
     * Screen dimensions. Ask termcap for its values if we haven't
     * already got any.
     */
    if (*pcolumns == 0) {
	int iv;

	iv = tgetnum("co");
	if (iv <= 0) {
	    fail("`co' entry in termcap is invalid or missing.");
	}
	*pcolumns = CO = (unsigned) iv;
    } else {
	CO = *pcolumns;
    }
    if (*prows == 0) {
	int iv;

	iv = tgetnum("li");
	if (iv <= 0) {
	    fail("`li' entry in termcap is invalid or missing.");
	}
	*prows = LI = (unsigned) iv;
    } else {
	LI = *prows;
    }

    /*
     * The optimisation calculations assume that "bc" and "up" are
     * one character long, dating from when xvi only used them if
     * they were one character long because it seemed "extremely
     * complicated to have to work out the number of characters used
     * to do a cursor move in every possible way".
     * Instead we use them anyway, maybe getting the calcs slightly wrong,
     * which seems better than not using them at all.
     */

    /* AIX's curses.h has no extern char PC, *BC or *UP */
#ifndef AIX
    cp = tgetstr("pc", &strp);	/* pad character */
    if (cp != NULL)
	PC = *cp;
#endif
    /*
     * Find the backspace character.
     *
     * Termcap capability "bs" should mean "you can move left with ^H" while
     * string capability "bc" means "how to move left if "bs" is not set."
     *
     * In NetBSD termcap's descriptions (the messiest!)
     *
     * 172 terminals have !bs !bc but le=, often "^H"!
     * 223 have :bs: and :le= but no bc=, for 200 le=^H, ansi has le=\E[D
     * 3 have no bs but bc= (^H, ^H and ^N)
     * 4 have bs, bc and le all defined:
     * 153 have !bs !bc but le=
     * 37 have none of them, of which 2 have :bw: (!).
     * 5 terminals have both bs and bc:
     *		   NetBSD termcap	emacs21 termcap   ncurses terminfo
     * dg6053:	   :bc=^Y:ho=^H:le=^Y:	:!bs:le=^Y:	  :bc=^Y:le=^Y:ho=^H:
     * hp9845:	   :bc=\ED:		:bc=\ED:	  :bc=\ED:le=\ED:
     * superbrain: :bc=^U:le=^H:	:bc=^U:le=^H:	  :bc=^U:le=^H:
     * z29:	   :bc=\ED:le=^H:	:bc=\ED:le=^H:	  :bc=\ED:le=^H:
     *
     * From the dg6053, if bs and bc, use bc.
     * From the 172, if there's neither bs nor bc, use le.
     * From ansi and the 223, if bs and le but no bc use ^H and ignore le.
     */
#ifndef AIX
    if ((BC = tgetstr("bc", &strp)) != NULL) {
	bc = BC;
    }
#else
    if ((cp = tgetstr("bc", &strp)) != NULL) {
	bc = cp;
    }
#endif
    else
    if (tgetflag("bs")) {
	bc = "\b";
    }
    else
    if ((cp = tgetstr("le", &strp)) != NULL) {
	bc = cp;
    }

    /*
     * Set other string capabilities
     */

#ifndef AIX
    if ((UP = tgetstr("up", &strp)) != NULL) {
	up = UP;
    }
#else
    up = tgetstr("up", &strp);
#endif

#ifndef	AIX
    /*
     * The termcap emulation (over terminfo) on an RT/PC
     * (the only AIX machine I have experience of) gets
     * the "do" capability wrong; it moves the cursor
     * down a line, but also sends a carriage return.
     * We must therefore avoid use of "do" under AIX.
     */

    down = tgetstr("do", &strp);	/* down a line */
#endif


    /*
     * Other strings.
     */
    cr = tgetstr("cr", &strp);
    nd = tgetstr("nd", &strp);
    KS = tgetstr("ks", &strp);
    KE = tgetstr("ke", &strp);
    VS = tgetstr("vs", &strp);
    VE = tgetstr("ve", &strp);
    TI = tgetstr("ti", &strp);
    TE = tgetstr("te", &strp);
    CE = tgetstr("ce", &strp);
    CL = tgetstr("cl", &strp);
    AL = tgetstr("al", &strp);
    DL = tgetstr("dl", &strp);
    IC = tgetstr("ic", &strp);
    IM = tgetstr("im", &strp);
    EI = tgetstr("ei", &strp);
    CM = tgetstr("cm", &strp);
    HO = tgetstr("ho", &strp);
    CS = tgetstr("cs", &strp);
    sf = tgetstr("sf", &strp);
    sr = tgetstr("sr", &strp);
    SF = tgetstr("SF", &strp);
    SR = tgetstr("SR", &strp);
    VB = tgetstr("vb", &strp);
    MR = tgetstr("mr", &strp);
    MD = tgetstr("md", &strp);
    ME = tgetstr("me", &strp);
    if (MR == NULL || MD == NULL || ME == NULL) {
	MD = NULL;
	MR = tgetstr("so", &strp);	/* assume "so" is reverse-mode */
	ME = tgetstr("se", &strp);	/* and use "se" to mean mode-end */
    }

    /*
     * Find up to 10 colour capabilities.
     */
    for (ncolours = 0; ncolours < 10; ncolours++) {
	char	capname[3];
	char	*cap;

	capname[0] = 'c';
	capname[1] = ncolours + '0';	/* assumes ASCII - nasty */
	capname[2] = '\0';
	cap = tgetstr(capname, &strp);
	if (cap == NULL)
	    break;
	colours[ncolours] = cap;
    }

    if (CM == NULL && HO == NULL) {
	fail("Xvi can't work without cursor motion or home.");
    }

    /*
     * Find out how many characters a "home" takes.
     * "ho" is always a fixed string (checked in NetBSD termcap file).
     */
    cost_home = HO   ? strlen(HO)   : NOCM;
    cost_cr   = cr   ? strlen(cr)   : NOCM;
    cost_down = down ? strlen(down) : NOCM;
    cost_bc   = bc   ? strlen(bc)   : NOCM;
    cost_up   = up   ? strlen(up)   : NOCM;

    /*
     * Set these variables as appropriate.
     */
    can_scroll_area = (
	(CS != NULL)
	&&
	(SF != NULL || sf != NULL || DL != NULL || can_movedown)
	&&
	(SR != NULL || sr != NULL || AL != NULL)
    );

    /*
     * Enter cursor arrow keys etc into xvi map table.
     */
    for (i = 0; keys[i].key_tcname != NULL; i++) {
	char	*lhs;

	lhs = tgetstr(keys[i].key_tcname, &strp);
	if (lhs != NULL) {
	    xvi_keymap(lhs, keys[i].key_rhs);
	}
    }
}

/*
 * Functions called to perform screen manipulations..
 */
static enum {
	m_SYS = 0,
	m_VI
}	termmode;

/*
 * Called by sys_startv(), just after switching to raw/cbreak mode.
 * Assumes tty_open() has been called.
 */
void
tty_startv()
{
    if (termmode == m_SYS) {
	if (TI != NULL)
	    tputs(TI, (int) LI, foutch);
	if (VS != NULL)
	    tputs(VS, (int) LI, foutch);
	if (KS != NULL)
	    tputs(KS, (int) LI, foutch);
    }
    old_colour = NO_COLOUR;
    pset_colour(&tcap_scr, VSCcolour);
    optimise = FALSE;
    termmode = m_VI;
}

/*
 * Called by sys_endv(), just before returning to cooked mode.
 *
 * tty_endv() can be called when we're already in system mode, so we
 * have to check.
 */
void
tty_endv()
{
    if (termmode == m_VI) {
	do_set_colour(VSCcolour);
	if (can_scroll_area) {
	    set_scroll_region(0, (int) LI - 1);
	}
	if (KE != NULL)
	    tputs(KE, (int) LI, foutch);
	if (VE != NULL)
	    tputs(VE, (int) LI, foutch);
	if (TE != NULL)
	    tputs(TE, (int) LI, foutch);
	flushout(&tcap_scr);
	termmode = m_SYS;
    }
}

/*
 * Erase from the cursor position to the end of the line
 */
void
erase_line()
{
    xyupdate();
    if (CE != NULL) {
	tputs(CE, (int) LI, foutch);
    } else {
	/*
	 * This happens when unix.c calls erase_line() in sys_endv() to clear
	 * the status line on a terminal without "ce" capability.
	 * We can't change erase_line() to clear_line() in sys_endv() because
	 * sunview doesn't have clear_line().
	 *
	 * As we know we are clearing the status line, don't do the last
	 * character of the line.
	 */
	while (real_col < CO-1) {
	    moutch(' ');
	    real_col++;
	}
    }
}

/*
 * Delete one line.
 */
void
delete_a_line()
{
    xyupdate();
    if (DL != NULL)
	tputs(DL, (int) LI, foutch);
}

/*
 * Erase display (may optionally home cursor).
 */
void
erase_display()
{
    /*
     * Don't know where the cursor goes, so turn optim
     * back off until we can re-sync the position.
     */
    optimise = FALSE;
    old_colour = NO_COLOUR;
    if (CL != NULL)
	tputs(CL, (int) LI, foutch);
    oflush();
}

/*
 * Internal routine: used to set the scroll region to an area
 * of the screen. We only change it if necessary.
 *
 * Assumes CS is available, i.e. can_scroll_area is TRUE.
 */
static void
set_scroll_region(top, bottom)
int	top, bottom;
{
    if (top != s_top || bottom != s_bottom) {
	tputs(tgoto(CS, bottom, top), bottom - top, foutch);
	s_top = top;
	s_bottom = bottom;
	/*
	 * Some terminals move the cursor when we set scroll region.
	 */
	optimise = FALSE;
    }
}

/*
 * Scroll up an area of the screen.
 */
void
scroll_up(start_row, end_row, nlines)
int	start_row, end_row, nlines;
{
    if (!can_scroll_area)
	return;

    /*
     * Set the scrolling region, if it is different
     * from the one there already. Note that we used
     * to set the scroll region after moving the cursor,
     * because we don't know how the terminal will
     * respond to movements when a scroll region is
     * set; some terminals move relative to the top
     * of the current scroll region. However, some
     * terminals (e.g.vt100) actually move the cursor
     * when the scroll region is set, so you can't win.
     */
    set_scroll_region(start_row, end_row);

    /*
     * Make sure we are in the "right" place before calling
     * xyupdate(), or we will get infinite recursion.
     * The "right" place is:
     *	if we have sf or SF:
     *		It would be nice to assume that "sf" / "SF"
     *		would work anywhere within the scroll region;
     *		unfortunately, this just isn't true. Sigh.
     *		So we have to put the cursor on the last row.
     *	if no sf or SF capability:
     *		if we have a dl capability:
     *			the first row of the area
     *			Only move the cursor to that row if it is
     *			not already there; this saves a lot of nasty
     *			"flicker" of the cursor.
     *		else:
     *			we have to use the "do" capability,
     *			on the bottom row of the scroll area.
     *			Assume it is safe to do this, because
     *			optimise will be turned off afterwards,
     *			and the caller save the cursor anyway.
     */
    if (SF == NULL && sf == NULL && DL != NULL) {
	if (virt_row != start_row) {
	    virt_row = start_row;
	    virt_col = 0;
	}
    } else /* use SF or sf or down */ {
	if (virt_row != end_row) {
	    virt_row = end_row;
	    virt_col = 0;
	}
    }

    xyupdate();

    /*
     * And scroll the area, either by an explicit sequence
     * or by the appropriate number of line insertions.
     */
    if (SF != NULL && (nlines > 1 || sf == NULL)) {
	tputs(tgoto(SF, nlines, nlines), end_row - start_row, foutch);
    } else if (sf != NULL) {
	int	i;

	for (i = 0; i < nlines; i++) {
	    tputs(sf, end_row - start_row, foutch);
	}
    } else if (DL != NULL) {
	int	i;

	for (i = 0; i < nlines; i++) {
	    tputs(DL, end_row - start_row, foutch);
	}
    } else {
	int	i;

	for (i = 0; i < nlines; i++) {
	    tputs(down, 1, foutch);
	}
    }

    /*
     * Set the scrolling region back to how it should be.
     */
    set_scroll_region(0, (int) LI - 1);

    /*
     * We don't know what this does to the cursor position;
     * so the safest thing to do here is to assume nothing.
     */
    optimise = FALSE;
}

/*
 * Scroll down an area of the screen.
 */
void
scroll_down(start_row, end_row, nlines)
int	start_row, end_row, nlines;
{
    if (CS == NULL || (SR == NULL && sr == NULL && AL == NULL))
	return;

    /*
     * Set the scrolling region, if it is different
     * from the one there already. Note that we used
     * to set the scroll region after moving the cursor,
     * because we don't know how the terminal will
     * respond to movements when a scroll region is
     * set; some terminals move relative to the top
     * of the current scroll region. However, some
     * terminals (e.g.vt100) actually move the cursor
     * when the scroll region is set, so you can't win.
     */
    set_scroll_region(start_row, end_row);

    /*
     * Make sure we are in the "right" place before calling
     * xyupdate(), or we will get infinite recursion.
     * The "right" place is:
     *	if no sr or SR capability:
     *		the first row of the area, so AL will work.
     *		Only move the cursor to that row if it is
     *		not already there; this saves a lot of nasty
     *		"flicker" of the cursor.
     *	if we have sr or SR:
     *		It would be nice to assume that "sr" / "SR"
     *		would work anywhere within the scroll region;
     *		unfortunately, this just isn't true. Sigh.
     *		So we use the first row here too.
     */
    if (virt_row != start_row) {
	virt_row = start_row;
	virt_col = 0;
    }

    xyupdate();

    /*
     * And scroll the area, either by an explicit sequence
     * or by the appropriate number of line insertions.
     */
    if (SR != NULL && (nlines > 1 || sr == NULL)) {
	tputs(tgoto(SR, nlines, nlines), end_row - start_row, foutch);
    } else if (sr != NULL) {
	int	i;

	for (i = 0; i < nlines; i++) {
	    tputs(sr, end_row - start_row, foutch);
	}
    } else {
	int	i;

	for (i = 0; i < nlines; i++) {
	    tputs(AL, end_row - start_row, foutch);
	}
    }

    /*
     * Set the scrolling region back to how it should be.
     */
    set_scroll_region(0, (int) LI - 1);

    /*
     * We don't know what this does to the cursor position;
     * so the safest thing to do here is to assume nothing.
     */
    optimise = FALSE;
}

/*
 * Insert the given character at the cursor position.
 */
void
inschar(c)
char	c;
{
    xyupdate();
    if (IC != NULL) {
	tputs(IC, (int) LI, foutch);
	outchar(c);
    } else if (IM != NULL) {
	tputs(IM, (int) LI, foutch);
	outchar(c);
	tputs(EI, (int) LI, foutch);
    }
}

/*
 * Goto the specified location.
 */
void
tty_goto(row, col)
int	row, col;
{
    virt_row = row;
    virt_col = col;
}


/*
 * This is an internal routine which is called whenever we want
 * to be sure that the cursor position is correct; it looks at
 * the cached values of the desired row and column, compares them
 * with the recorded "real" row and column, and outputs whatever
 * escape codes are necessary to put the cursor in the right place.
 *
 * Some optimisation is done here, because quite often this can be
 * a big win; many cursor movements asked for by the editor are
 * quite simple to do, only costing a few output bytes and little
 * work.  If "optimise" is FALSE, this optimisation is not done.
 * Other routines in this file can use this to turn optimisation
 * off temporarily if they "lose" the cursor.
 */

/*
 * Helper routines for xyudate() tell you how many characters it would take
 * to move the cursor from real_{row,col} to virt_{row,col} using a particular
 * motion strategy:
 *
 * cm_h_only()        Just the horizontal part, using relative motions
 * cm_v_only()        Just the vertical part, using relative motions
 * cm_home_relative() All of it outputting Home and relative motions
 * cm_relative()      All of it just using relative motions (and/or CR)
 * cm_gotoxy()	      All of it using a single cursor motion sequence
 *
 * Notice that "carriage return and move right" is included in cm_h_only()
 * and hence in cm_relative() (despite its name).
 *
 * They all take a parameter "doit":
 * - If "FALSE, they calculate and return the number of characters that would
 *   be required to perform this motion with the specified strategy;
 * - If TRUE, they output their best motion sequence.
 *
 * There are also
 * ch	Move cursor horizontally only to column %1
 * cv   Move cursor vertically only to line %1
 */


/*
 * Helper function for the helper functions:
 * move right by redrawing the characters already on the screen.
 */
static int
cm_right_by_redraw(bool_t doit)
{
    int cost = virt_col - real_col;

    if (doit) {
	VirtScr *vs = curwin->w_vs;
	Sline *sline = &(vs->pv_ext_lines[real_row]);
	int *pv_colours = vs->pv_colours;

	while (real_col < virt_col) {
	    unsigned char colour;
	    char ch;
	    if (real_col >= sline->s_used) {
		colour = VSCcolour;
		ch = ' ';
	    } else {
		colour = pv_colours[sline->s_colour[real_col]];
		ch = sline->s_line[real_col];
	    }
	    do_set_colour(colour);
	    moutch(ch);
	    real_col++;
	}
    }
    return(cost);
}

/*
 * Find the cost of, or perform, a horizontal-only motion
 * from real_col to virt_col.
 *
 * Strategies are:
 * - one or more single character motions using "bs" to go left
 *   or rewriting the characters already on the screen to go right;
 * - using a single LE or RI motion (move left/right by N chars)
 */
static int
cm_h_only(doit)
bool_t	doit;
{
    if (real_col == virt_col) return(0);

    /* Moving left */
    if (real_col > virt_col) {
        int crcost = NOCM;
        int bscost = NOCM;

	/* Calculate the cost of doing it with a carriage return and
	 * re-outputting screen characters to move right */
	if (cr != NULL) {
	    crcost = cost_cr + virt_col;
	}

	/* Calculate the cost of doing it with N backspaces */
	if (can_backspace) {
	    bscost = cost_bc * (real_col - virt_col);
	}

	/*
	 * Choose a motion strategy.
	 * The <'s ensure that if both are NOCM, neither is chosen and
	 * prefer the later options to the earlier if they cost the same.
	 */
	/* First option: Output carriage return and redraw screen characters */
	if (crcost < bscost) {
	    if (doit) {
		tputs(cr, 1, foutch);
		real_col = 0;
	        return(cm_right_by_redraw(TRUE));
	    } else {
	        return(crcost);
	    }
	}

	/* Second option: Backspaces */
	if (bscost != NOCM) {
	    if (doit) {
		while (virt_col < real_col) {
		    tputs(bc, 1, foutch);
		    real_col--;
		}
	    }
	    return(bscost);
	}
    }

    /* Moving right */
    if (virt_col > real_col) {
	return(cm_right_by_redraw(doit));
    }

    /* Can't do this */
    return(NOCM);
}

/* Find the cost of, or perform, a vertical motion using relative motions */
static int
cm_v_only(doit)
bool_t	doit;
{
    if (real_row == virt_row) return(0);

    /* Moving up */
    if (real_row > virt_row) {
	if (up != NULL) {
	    int cost = cost_up * (real_row - virt_row);
	    if (doit) {
		while (virt_row < real_row) {
		    tputs(up, 1, foutch);
		    real_row--;
		}
	    }
	    return(cost);
	}
    }

    /* Moving down */
    if (virt_row > real_row) {
	if (down != NULL) {
	    int cost = (virt_row - real_row) * cost_down;
	    if (doit) {
		while (virt_row > real_row) {
		    tputs(down, 1, foutch);
		    real_row++;
		}
	    }
	    return(cost);
	}
    }

    /* Can't do this */
    return(NOCM);
}

static int	cm_relative P((bool_t));

/*
 * Do an absolute motion, the only option when !optimise.
 *
 * We used to compare the cost of a CM string with that of
 * going Home and issuing relative motions, but the win is so rare
 * and so small that we don't bother. We use that strategy as a backup
 * for when there is no CM capabillity.
 */
static int
cm_absolute(doit)
bool_t	doit;
{
    if (CM != NULL) {
	cost = 0;	/* the global one */
	tputs(tgoto(CM, virt_col, virt_row), (int)LI, doit ? foutch : inc_cost);
	if (doit) {
	    real_row = virt_row;
	    real_col = virt_col;
	}
	return(cost);
    }

    if (HO != NULL) {
	int cost = cost_home;
	int save_real_col = real_col;
	int save_real_row = real_row;

	if (doit) {
	    tputs(HO, 1, foutch);
	}
	real_col = real_row = 0;    /* if !doit, just pretend we went home */
	cost += cm_relative(doit);
	if (!doit) {
	    real_col = save_real_col;
	    real_row = save_real_row;
	}
	return(cost);
    }

    return(NOCM);
}

/*
 * Perform/measure a motion performed by doing the horizontal and the
 * vertical motions separately. The name is a misnomer as it may use
 * carriage-return.
 */
static int
cm_relative(doit)
bool_t	doit;
{
    int cost;

    /* We can only do relative motion if we know where the cursor is! */
    if (!optimise) return(NOCM);

    /*
     * Move to the line first, otherwise if moving from an empty line
     * to a full one, c_right_by_redraw() moves over blanks instead of
     * characters, which is visually less pleasing.
     */
    cost = cm_v_only(doit);
    cost += cm_h_only(doit);
    return(cost);
}

static void
xyupdate()
{
    register int	hdisp, vdisp;

    /*
     * Horizontal and vertical displacements needed
     * to get the cursor to the right position.
     * These are positive for downward and rightward movements.
     *
     * Totaldisp is the total absolute displacement.
     */
    hdisp = virt_col - real_col;
    vdisp = virt_row - real_row;

    if (hdisp == 0 && vdisp == 0 && optimise) {
	 return;
    };

    /*
     * First, ensure that the current scroll region
     * contains the intended cursor position.
     */
    if (can_scroll_area && (virt_row < s_top || virt_row > s_bottom)) {
	set_scroll_region(0, (int) LI - 1);
    }


    /*
     * Now choose the best optimised strategy and apply it.
     */
    {
	int abscost = cm_absolute(FALSE);
	int relcost = cm_relative(FALSE);

	/*
	 * We are only allowed to move when set to colour 0
	 * or some terminals will write garbage all over the screen.
	 *
	 * Exception: when moving right by rewriting screen characters.
	 */
	if (!can_move_in_standout &&
	    !(optimise && vdisp==0 && hdisp > 0 && relcost < abscost)) {
	    do_set_colour(VSCcolour);
	}

	/*
	 * These <'s disable a candidate if both it and its competitor are
	 * unavailable and in case of a match, choose the later candidate.
	 * We prefer absolute motions to relative ones.
	 */
	if (relcost < abscost) {
	    cm_relative(TRUE);
	} else {
	    cm_absolute(TRUE);
	}
    }
    optimise = TRUE;
}
