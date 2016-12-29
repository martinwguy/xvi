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
	DO	str	down N lines
	up	str	Upline (cursor up)
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
	ms	bool	Safe to move while in standout and underline mode

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
static	void		clear_rows P((VirtScr *, int, int));
static	void		clear_line P((VirtScr *, int, int));
static	void		xygoto P((VirtScr *, int, int));
static	void		xyadvise P((VirtScr *, int, int, int, char *));
static	void		put_str P((VirtScr *, int, int, char *));
static	void		put_char P((VirtScr *, int, int, int));
static	void		ins_str P((VirtScr *, int, int, char *));
static	void		pset_colour P((VirtScr *, int));
static	int		colour_cost P((VirtScr *));
static	int		scroll P((VirtScr *, int, int, int));
static	int		can_scroll P((VirtScr *, int, int, int));
static	void		flushout P((VirtScr *));
static	void		pbeep P((VirtScr *));

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
int		cost_goto = 0;		/* cost of doing a goto */
bool_t		can_scroll_area = FALSE; /* true if we can set scroll region */
bool_t		can_del_line;		/* true if we can delete lines */
bool_t		can_ins_line;		/* true if we can insert lines */
bool_t		can_inschar;		/* true if we can insert characters */
unsigned int	CO = 0;			/* screen dimensions; 0 at start */
unsigned int	LI = 0;

/*
 * Needed by termcap library.
 */
char	PC;				/* pad character */

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

static	char	bc;			/* backspace char */
static	char	ND;			/* non-destructive forwward space */
static	char	DO;			/* down one line */

static	bool_t	can_backspace;		/* true if can backspace (bs/bc) */
static	bool_t	can_fwdspace = FALSE;	/* true if can forward space (nd) */
static	bool_t	can_movedown = FALSE;	/* true if can move down (do) */
static	bool_t	auto_margins;		/* true if AM is set */

/*
 * We use this table to perform mappings from cursor keys
 * into appropriate xvi input keys (in command mode).
 */
static unsigned char arrow_keys[] = {
    K_UARROW,	'\0',
    K_DARROW,	'\0',
    K_RARROW,	'\0',
    K_LARROW,	'\0',
    CTRL('B'),	'\0',
    CTRL('F'),	'\0',
    K_HELP,	'\0',
};
static struct {
    char	*key_tcname;
    char	*key_rhs;
} keys[] = {
  { "ku",	(char *) arrow_keys + 0		}, /* up */
  { "kd",	(char *) arrow_keys + 2		}, /* down */
  { "kr",	(char *) arrow_keys + 4		}, /* right */
  { "kl",	(char *) arrow_keys + 6		}, /* left */
  { "kP",	(char *) arrow_keys + 8		}, /* page up */
  { "kN",	(char *) arrow_keys + 10	}, /* page down */
  { "kh",	"H"				}, /* home */
  { "k0",	"#0"				}, /* function key 0 */
  { "k1",	(char *) arrow_keys + 12	}, /* help */
  { "k2",	"#2"				}, /* function key 2 */
  { "k3",	"#3"				}, /* function key 3 */
  { "k4",	"#4"				}, /* function key 4 */
  { "k5",	"#5"				}, /* function key 5 */
  { "k6",	"#6"				}, /* function key 6 */
  { "k7",	"#7"				}, /* function key 7 */
  { "k8",	"#8"				}, /* function key 8 */
  { "k9",	"#9"				}, /* function key 9 */
  { NULL,	NULL				}
};

/*
 * Standout glitch: number of spaces left when entering or leaving
 * standout mode.
 *
 * This abomination is still needed for some terminals (e.g.
 * Televideo).
 */
int		SG;

/*
 * Used by scroll region optimisation.
 */
static int	s_top = 0, s_bottom = 0;

/*
 * Used for colour-setting optimisation.
 */
#define	NO_COLOUR	-1
static	int		old_colour = NO_COLOUR;

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

    ignore_signals();
    if (xvi_startup(vs, argc, argv, getenv("XVINIT")) == NULL) {
	sys_endv();
	exit(1);
    }
    catch_signals();

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

    event.ev_vs = vs;
    while (1) {
	xvResponse	*resp;
	register int	r;

	r = inch(timeout);
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
	    } else if (win_size_changed) {
		unsigned	new_rows, new_cols;

		win_size_changed = FALSE;

		getScreenSize(&new_rows, &new_cols);
		if (new_rows != 0 && new_cols != 0) {
		    event.ev_type = Ev_resize;
		    event.ev_rows = new_rows;
		    event.ev_columns = new_cols;
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
    if (colour == old_colour) {
	return;
    }

    xyupdate();

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
	 *	tagcolour	4
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
#ifdef SG_TEST
		outchar('-');
#endif
		tputs(ME, 1, foutch);
	    }
	    break;

	case 4:				/* double intensity */
	    if (MD != NULL) {
		tputs(MD, 1, foutch);
#ifdef SG_TEST
		outchar('+');
#endif
		break;
	    }
	    /* else FALL THROUGH */

	default:			/* reverse */
	    if (MR != NULL) {
		tputs(MR, 1, foutch);
#ifdef SG_TEST
		outchar('+');
#endif
	    }
	}
    }

    old_colour = colour;
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
	/*
	 * nlines negative means scroll reverse - i.e. move
	 * the text downwards with respect to the terminal.
	 */
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
			moutch('\n');
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
    real_col++;
    virt_col++;
    if (real_col >= CO) {
	if (auto_margins) {
	    virt_col = (real_col = 0);
	    virt_row = (real_row += 1);
	} else {
	    optimise = FALSE;
	}
    }
    moutch(c);
}

/*
 * Put out a "normal" string, updating the cursor position.
 */
void
outstr(s)
register char	*s;
{
    xyupdate();
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
    if (real_col >= CO) {
	if (auto_margins) {
	    virt_col = (real_col %= CO);
	    virt_row = (real_row += 1);
	} else {
	    optimise = FALSE;
	}
    }
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

/*
 * Look up term entry in termcap database, and set up all the strings.
 */
void
tty_open(prows, pcolumns)
unsigned int	*prows;
unsigned int	*pcolumns;
{
    char	tcbuf[1024];		/* buffer for termcap entry */
    char	*termtype;		/* terminal type */
    static	char strings[512];	/* space for storing strings */
    char	*strp = strings;	/* ptr to space left in strings */
    char	*cp;			/* temp for single char strings */
    int	i;

    termtype = getenv("TERM");
    if (termtype == NULL) {
	fail("Can't find your terminal type.");
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
    auto_margins = (bool_t) (tgetflag("am") && !tgetflag("xn"));
    can_backspace = (bool_t) tgetflag("bs");

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

    SG = tgetnum("sg");
    if (SG < 0) {
	SG = 0;
    }
#ifdef SG_TEST
    SG++;
#endif

    /*
     * Single-char strings - some of these may be strings,
     * but we only want them if they are single characters.
     * This is because the optimisation calculations get
     * extremely complicated if we have to work out the
     * number of characters used to do a cursor move in
     * every possible way; we basically assume that we
     * don't have infinite amounts of time or space.
     */
    cp = tgetstr("pc", &strp);	/* pad character */
    if (cp != NULL)
	PC = *cp;

    cp = tgetstr("bc", &strp);	/* backspace char if not ^H */
    if (cp != NULL && cp[1] == '\0')
	bc = *cp;
    else
	bc = '\b';

    cp = tgetstr("nd", &strp);	/* non-destructive forward space */
    if (cp != NULL && cp[1] == '\0') {
	ND = *cp;
	can_fwdspace = TRUE;
    }

#ifndef	AIX
    /*
     * The termcap emulation (over terminfo) on an RT/PC
     * (the only AIX machine I have experience of) gets
     * the "do" capability wrong; it moves the cursor
     * down a line, but also sends a carriage return.
     * We must therefore avoid use of "do" under AIX.
     */

    cp = tgetstr("do", &strp);	/* down a line */
    if (cp != NULL && cp[1] == '\0') {
	DO = *cp;
	can_movedown = TRUE;
    }
#endif

    /*
     * Strings.
     */
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

    if (CM == NULL) {
	fail("Xvi can't work without cursor motion.");
    }

    /*
     * This may not be quite right, but it will be close.
     */
    cost_goto = strlen(CM) - 1;

    /*
     * Set these variables as appropriate.
     */
    can_del_line = (DL != NULL);
    can_ins_line = (AL != NULL);
    can_inschar = (IC != NULL) || (IM != NULL);
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
    VSset_colour(&tcap_scr, VSCcolour);
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
	VSset_colour(&tcap_scr, VSCsyscolour);
	if (can_scroll_area) {
	    set_scroll_region(0, (int) LI - 1);
	}
	if (KE != NULL)
	    tputs(KE, (int) LI, foutch);
	if (VE != NULL)
	    tputs(VE, (int) LI, foutch);
	if (TE != NULL)
	    tputs(TE, (int) LI, foutch);
	termmode = m_SYS;
    }
    oflush();
}

/*
 * Erase the entire current line.
 */
void
erase_line()
{
    xyupdate();
    if (CE != NULL)
	tputs(CE, (int) LI, foutch);
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
    } else /* use SF or sf or DO */ {
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
	    moutch(DO);
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
static void
xyupdate()
{
    register int	hdisp, vdisp;
    register int	totaldisp;

    /*
     * Horizontal and vertical displacements needed
     * to get the cursor to the right position.
     * These are positive for downward and rightward movements.
     *
     * Totaldisp is the total absolute displacement.
     */
    hdisp = virt_col - real_col;
    vdisp = virt_row - real_row;

    totaldisp = ((vdisp < 0) ? -vdisp : vdisp) +
	    ((hdisp < 0) ? -hdisp : hdisp);

    /*
     * First, ensure that the current scroll region
     * contains the intended cursor position.
     */
    if (virt_row < s_top || virt_row > s_bottom) {
	if (can_scroll_area)
	    set_scroll_region(0, (int) LI - 1);
    }

    /*
     * If we want to go near the top of the screen, it may be
     * worth using HO.  We musn't do this if we would thereby
     * step outside the current scroll region.  Also, we can
     * only move to a position near home if we can use "down"
     * and "right" movements after having gone to "home".
     */
    if (
	(
	    HO != NULL		/* "home" capability exists */
	)
	&&				/* AND */
	(
	    !can_scroll_area	/* no scroll regions */
	    ||
	    s_top == 0		/* or doesn't affect us */
	)
	&&				/* AND */
	(
	    virt_col == 0		/* we're not moving right */
	    ||
	    can_fwdspace		/* or we can if we want */
	)
	&&				/* AND */
	(
	    virt_row == 0		/* we're not moving down */
	    ||
	    can_movedown		/* or we can if we want */
	)
    ) {
	/*
	 * Cost of using the "ho" capability.
	 */
	static unsigned		cost_home;

	/*
	 * Possible total cost of getting to the desired
	 * position if we use "ho".
	 */
	register unsigned	netcost;

	if (cost_home == 0)
	    cost_home = strlen(HO);
	netcost = cost_home + virt_row + virt_col;

	/*
	 * Only use home if it is worth it, and if
	 * either we are already below where we want
	 * to be on the screen, or optimise is off
	 * (and hence relative movements inappropriate).
	 */
	if (netcost < cost_goto
	    &&
	    (!optimise || real_row > virt_row)) {
	    tputs(HO, (int) LI, foutch);
	    real_row = real_col = 0;
	    totaldisp = (hdisp = virt_col) + (vdisp = virt_row);
	    optimise = TRUE;
	}
    }

    if (!optimise) {
	/*
	 * If optim is off, we should just go to the
	 * specified place; we can then turn it on,
	 * because we know where the cursor is.
	 */
	tputs(tgoto(CM, virt_col, virt_row), (int) LI, foutch);
	optimise = TRUE;
    } else {
	if (vdisp != 0 || hdisp != 0) {
	    /*
	     * Update the cursor position in the best way.
	     */

	    if (
		(totaldisp < cost_goto)
		&&
		(
		    hdisp == 0
		    ||
		    (hdisp > 0 && can_fwdspace)
		    ||
		    (hdisp < 0 && can_backspace)
		)
		&&
		(
		    vdisp >= 0
		    &&
		    can_movedown
		)
	    ) {
		/*
		 * A small motion; worth looking at
		 * doing it with BS, ND and DO.
		 * No UP handling yet, and we don't
		 * really care about whether ND is
		 * more than one char - this can make
		 * the output really inefficient.
		 */

		int	n;

		/*
		 * Move down to the right line.
		 */
		for (n = vdisp; n > 0; n--) {
		    moutch(DO);
		}

		if (hdisp < 0) {
		    if (virt_col == 0) {
			moutch('\r');
		    } else {
			for (n = hdisp; n < 0; n++) {
			    moutch(bc);
			}
		    }
		} else if (hdisp > 0) {
		    for (n = hdisp; n > 0; n--) {
			moutch(ND);
		    }
		}

	    } else if (vdisp == 0) {

		/*
		 * Same row.
		 */
		if (virt_col == 0) {
		    /*
		     * Start of line - easy.
		     */
		    moutch('\r');

		} else if (can_fwdspace && hdisp > 0 &&
			    hdisp < cost_goto) {
		    int	n;

		    /*
		     * Forward a bit.
		     */
		    for (n = hdisp; n > 0; n--) {
			moutch(ND);
		    }

		} else if (can_backspace && hdisp < 0 &&
			    (-hdisp) < cost_goto) {
		    int	n;

		    /*
		     * Back a bit.
		     */
		    for (n = hdisp; n < 0; n++) {
			moutch(bc);
		    }

		} else {
		    /*
		     * Move a long way.
		     */
		    tputs(tgoto(CM, virt_col, virt_row),
				1, foutch);
		}
	    } else if (virt_col == 0) {

		/*
		 * Different row, column 0.
		 */
		if (vdisp > 0 && vdisp + 1 < cost_goto) {
		    /*
		     * Want to move downwards.
		     * This happens a lot.
		     */
		    int	n;

		    if (real_col != 0)
			moutch('\r');
		    for (n = vdisp; n > 0; n--) {
			moutch('\n');
		    }
		} else {
		    /*
		     * Want to move upwards.
		     */
		    tputs(tgoto(CM, virt_col, virt_row),
			    (int) LI, foutch);
		}
	    } else {
		/*
		 * Give up - do a goto.
		 */
		tputs(tgoto(CM, virt_col, virt_row),
			(int) LI, foutch);
	    }
	}
    }
    real_row = virt_row;
    real_col = virt_col;
}
