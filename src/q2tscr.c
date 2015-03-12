/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    q2tscr.c
* module function:
    VirtScr interface for QNX Windows on QNX2.15 or later,
    using the CII C86 ANSI C Compiler.

    Must also link in qnx.c, which provides the system interface functions.

    We assume newscr() is only called once; it is an
    error for it to be called more than once.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include <io.h>
#include <fileio.h>
#undef	FORWARD				/* avoid conflict with xvi.h */
#include <dev.h>
#include <magic.h>
#include <process.h>
#include <systids.h>
#include <timer.h>
#include <tcap.h>
#include <tcapkeys.h>
#include <taskmsgs.h>

#include "xvi.h"

/*
 * This tells the editor whether we can handle subshell invocation.
 */
const	bool_t		subshells = TRUE;

static	VirtScr		*newscr P((VirtScr *, genptr *));
static	void		closescr P((VirtScr *));
static	void		clear_all P((VirtScr *));
static	void		clear_rows P((VirtScr *, int, int));
static	void		clear_line P((VirtScr *, int, int));
static	void		xygoto P((VirtScr *, int, int));
static	void		xyadvise P((VirtScr *, int, int, int, char *));
static	void		put_str P((VirtScr *, int, int, char *));
static	void		put_char P((VirtScr *, int, int, int));
static	void		pset_colour P((VirtScr *, int));
static	int		colour_cost P((VirtScr *));
static	int		co_scroll P((VirtScr *, int, int, int));
static	int		vt_scroll P((VirtScr *, int, int, int));
static	int		win_scroll P((VirtScr *, int, int, int));
static	void		flushout P((VirtScr *));
static	void		pbeep P((VirtScr *));

extern	int		inchar(long);
extern	void		tty_init(void);

VirtScr	qnxscr = {
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

    NOFUNC,		/* v_insert	    */
    NOFUNC,		/* v_scroll	    */
    NOFUNC,		/* v_flash	    */
    NOFUNC,		/* v_can_scroll     */
};

int			qnx_disp_inited = 0;

static	unsigned	screen_colour;

static	bool_t		triggered;

int
main(argc, argv)
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
    tty_init();

    ignore_signals();
    if (xvi_startup(&qnxscr, argc, argv, getenv("XVINIT")) == NULL) {
	sys_endv();
	exit(1);
    }
    catch_signals();

    /*
     * Set default colour to start with.
     */
    screen_colour = qnxscr.pv_colours[VSCcolour];

    event.ev_vs = &qnxscr;
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

void
startup_error(str)
char	*str;
{
    static int	called = 0;

    if (!called) {
    	sys_endv();
	called = 1;
    }
    (void) fputs(str, stderr);
}

void
tty_init()
{
    char	lhs[2];
    char	rhs[3];
    int		i;

    if (!qnx_disp_inited) {
	term_load(stdin);
    }

    /*
     * Work out what kind of terminal we are on, and try to get more information
     * about the screen size if appropriate. Also set up scrolling function.
     */
    if (strcmp(tcap_entry.term_name, "qnx") == 0) {
    	qnxscr.v_scroll = co_scroll;

	if (tcap_entry.term_type == VIDEO_MAPPED) {
	    unsigned	console;
	    unsigned	row_col;

	    console = fdevno(stdout);
	    row_col = video_get_size(console);
	    tcap_entry.term_num_rows = (row_col >> 8) & 0xff;
	    tcap_entry.term_num_cols = row_col & 0xff;
	}

	/*
	 * Don't do this until after we have got the screen size.
	 */
	term_video_on();

    } else if (strcmp(tcap_entry.term_name, "qnxw") == 0) {
	static	void	window_info(FILE *fp);

	window_info(stdout);

    	qnxscr.v_scroll = win_scroll;

    } else if (strncmp(tcap_entry.term_name, "vt100", 5) == 0) {
    	qnxscr.v_scroll = vt_scroll;
    }

    qnx_disp_inited = 1;

    sys_startv();

    qnxscr.pv_rows = (unsigned) tcap_entry.term_num_rows;
    qnxscr.pv_cols = (unsigned) tcap_entry.term_num_cols;

    /*
     * Insert keymaps for function keys 2 to 9.
     */
    lhs[1] = '\0';
    rhs[0] = '#';
    rhs[2] = '\0';
    for (i = 2; i <= 9; i++) {
	lhs[0] = K_FUNC(i);
	rhs[1] = i + '0';
    	xvi_keymap(lhs, rhs);
    }
}

/*
 * End visual mode. This is called by sys_endv() before we go to the shell.
 */
void
tty_endv()
{
    screen_colour = qnxscr.pv_colours[VSCsyscolour];
    term_colour(screen_colour >> 8);
    term_cur(tcap_entry.term_num_rows - 1, 0);
    term_clear(_CLS_EOL);
    fflush(stdout);
}

static void
catch_usr1(i)
int	i;
{
    (void) signal(SIGUSR1, SIG_IGN);
    triggered = TRUE;
}

/*
 * inchar() - get a character from the keyboard
 */
int
inchar(timeout)
long	timeout;
{
    register int	c;
    bool_t		setsignal = FALSE;

    /*
     * Set a timeout on input if asked to do so.
     */
    if (timeout != 0) {
	/*
	 * Timeout value is in milliseconds, so we have to convert
	 * to 50 millisecond ticks, rounding up as we do it.
	 */
	(void) signal(SIGUSR1, catch_usr1);
	(void) set_timer(TIMER_SET_EXCEPTION, RELATIVE,
			(int) ((timeout + 19) / 20),
			SIGUSR1,
			(char *) NULL);
	setsignal = TRUE;
    }

    fflush(stdout);
    for (;;) {
	triggered = FALSE;

	c = term_key();

	if (triggered) {
	    return(EOF);
	}

	if (c > 127) {
	    /*
	     * Must be a function key press.
	     */

	    if (State != NORMAL) {
		/*
		 * Function key pressed during insert
		 * or command line mode.
		 */
		pbeep(&qnxscr);
		continue;
	    }

	    switch (c) {
	    case KEY_F1: /* F1 key */
		c = K_HELP;
		break;
	    case (KEY_F1 + 1): case (KEY_F1 + 2): case (KEY_F1 + 3):
	    case (KEY_F1 + 4): case (KEY_F1 + 5): case (KEY_F1 + 6):
	    case (KEY_F1 + 7): case (KEY_F1 + 8): case (KEY_F1 + 9):
		c = K_FUNC(c + 1 - KEY_F1);
		break;
	    case KEY_HOME:
		c = 'H';
		break;
	    case KEY_END:
		c = 'L';
		break;
	    case KEY_PAGE_UP:
		c = CTRL('B');
		break;
	    case KEY_PAGE_DOWN:
		c = CTRL('F');
		break;
	    case KEY_UP:
		c = K_UARROW;
		break;
	    case KEY_LEFT:
		c = K_LARROW;
		break;
	    case KEY_RIGHT:
		c = K_RARROW;
		break;
	    case KEY_DOWN:
		c = K_DARROW;
		break;
	    case KEY_WORD_LEFT:
		c = 'b';
		break;
	    case KEY_WORD_RIGHT:
		c = 'w';
		break;
	    case KEY_INSERT:
		c = 'i';
		break;
	    case KEY_DELETE:
		c = 'x';
		break;
	    case KEY_RUBOUT:
		c = '\b';
		break;
	    case KEY_TAB:
		c = '\t';
		break;
	    }
	}
	if (setsignal)
	    (void) signal(SIGUSR1, SIG_IGN);
	return(c);
    }
    /*NOTREACHED*/
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
    term_clear(_CLS_SCRH);
}

/*ARGSUSED*/
static void
clear_rows(scr, start, end)
VirtScr	*scr;
int	start;
int	end;
{
    if (start == 0 && end == (scr->pv_rows - 1)) {
	term_clear(_CLS_SCRH);
    } else {
    	int	row;

	for (row = start; row <= end; row++) {
	    term_cur(row, 0);
	    term_clear(_CLS_EOL);
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
    term_cur(row, col);
    if (col < scr->pv_cols) {
	term_clear(_CLS_EOL);
    }
}

/*ARGSUSED*/
static void
xygoto(scr, row, col)
VirtScr	*scr;
int	row;
int	col;
{
    term_cur(row, col);
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
    /*
     * Assume a console for the moment.
     */
}

/*ARGSUSED*/
static void
put_str(scr, row, col, str)
VirtScr	*scr;
int	row;
int	col;
char	*str;
{
    term_type(row, col, str, 0, screen_colour);
}

/*ARGSUSED*/
static void
put_char(scr, row, col, c)
VirtScr	*scr;
int	row;
int	col;
int	c;
{
    char	cbuf[2];

    cbuf[0] = c;
    cbuf[1] = '\0';
    term_type(row, col, cbuf, 0, screen_colour);
}

/*ARGSUSED*/
static void
pset_colour(scr, colour)
VirtScr	*scr;
int	colour;
{
    screen_colour = (unsigned) colour;
    term_colour(screen_colour >> 8);
}

/*ARGSUSED*/
static int
colour_cost(scr)
VirtScr	*scr;
{
    return(0);
}

/*
 * Scroll an area on the console screen up or down by nlines.
 * We can assume that we will not be called with parameters
 * which would result in scrolling the entire window.
 */
/*ARGSUSED*/
static int
co_scroll(scr, start_row, end_row, nlines)
VirtScr	*scr;
int	start_row;
int	end_row;
int	nlines;
{
    char	*buf;
    int		i;

    buf = malloc((unsigned) scr->pv_cols * term_save_image(0, 0, NULL, 0));
    if (buf == NULL)
	return(0);

    if (nlines < 0) {
	/*
	 * nlines negative means scroll reverse - i.e. move
	 * the text downwards with respect to the terminal.
	 */
	nlines = -nlines;

	for (i = end_row; i >= start_row + nlines; --i) {
	    term_save_image(i - nlines, 0, buf, scr->pv_cols);
	    term_restore_image(i, 0, buf, scr->pv_cols);
	}

	for ( ; i >= start_row; --i) {
	    term_cur(i, 0);
	    term_clear(_CLS_EOL);
	}
    } else if (nlines > 0) {
	/*
	 * Whereas nlines positive is "normal" scrolling.
	 */
	for (i = start_row; i <= end_row - nlines; i++) {
	    term_save_image(i + nlines, 0, buf, scr->pv_cols);
	    term_restore_image(i, 0, buf, scr->pv_cols);
	}

	for ( ; i <= end_row; i++) {
	    term_cur(i, 0);
	    term_clear(_CLS_EOL);
	}
    }

    free(buf);

    return(1);
}

/*
 * Scroll an area on a vt100 terminal screen by nlines.
 */
static int
vt_scroll(scr, start_row, end_row, nlines)
VirtScr	*scr;
int	start_row;
int	end_row;
int	nlines;
{
    int		count;

    (void) printf("\0337");				/* save cursor */
    (void) printf("\033[%d;%dr", start_row + 1,
    					end_row + 1);	/* set scroll region */
    if (nlines < 0) {
	/*
	 * nlines negative means scroll reverse - i.e. move
	 * the text downwards with respect to the terminal.
	 */
	nlines = -nlines;

	(void) printf("\033[%d;1H", start_row + 1);	/* goto top left */
	for (count = 0; count < nlines; count++) {
	    fputs("\033[L", stdout);
	}

    } else if (nlines > 0) {
	/*
	 * Whereas nlines positive is "normal" scrolling.
	 */
	(void) printf("\033[%d;1H", end_row + 1);	/* goto bottom left */
	for (count = 0; count < nlines; count++) {
	    putchar('\n');
	}
    }
    (void) printf("\033[1;%dr", scr->pv_rows);		/* set scroll region */
    (void) printf("\0338");				/* restore cursor */
    return(1);
}

/*
 * Attempt to scroll an area in a wterm window by nlines.
 * We can only scroll the whole window.
 */
static int
win_scroll(scr, start_row, end_row, nlines)
VirtScr	*scr;
int	start_row;
int	end_row;
int	nlines;
{
    int		count;
    char	*esc;

    if (end_row != scr->pv_rows - 1) {
    	return(0);
    }

    if (nlines < 0) {
	/*
	 * nlines negative means scroll reverse - i.e. move
	 * the text downwards with respect to the terminal.
	 *
	 * We don't use the disp_scroll_down capability here
	 * because it doesn't seem to work.
	 */
	esc = tcap_entry.disp_insert_line;
	if (esc[0] == '\0') {
	    return(0);
	}

	nlines = -nlines;
	term_cur(start_row, 0);

    } else if (nlines > 0) {
	/*
	 * Whereas nlines positive is "normal" scrolling.
	 */
	esc = tcap_entry.disp_delete_line;
	if (esc[0] != '\0') {
	    term_cur(start_row, 0);
	} else {
	    if (start_row != 0) {
		return(0);
	    }
	    esc = "\n";
	    term_cur(end_row, 0);
	}
    }

    for (count = 0; count < nlines; count++) {
	term_esc(esc);
    }
    return(1);
}

/*ARGSUSED*/
static void
flushout(scr)
VirtScr	*scr;
{
    (void) fflush(stdout);
}

/*ARGSUSED*/
static void
pbeep(scr)
VirtScr	*scr;
{
    putchar('\007');
    fflush(stdout);
}

/*
 * Window size information code. This is a vile hack.
 */
struct winmsg {
int	wid,			/* window id */
	pid,			/* picture id */
	row,			/* cursor row */
	col,			/* cursor column */
	mrows,			/* absolute maximum rows */
	mcols,			/* absolute maximum colums */
	flags,			/* internal windows flags */
	rsv1[2],		/* reserved for future */
	prows,			/* physical rows displayed */
	pcols,			/* physical columns displayed */
	lrows,			/* logical maximum rows */
	lcols,			/* logical maximun columns */
	drow,			/* display row offset */
	dcol,			/* display column offset */
	wflags,			/* user setable windows flags */
	wflags2,		/* user setable windows flags */
	rsv2[3];		/* reserved for future */
} winmsg;

#ifndef __WINDOW_INFO
#define __WINDOW_INFO	34
#endif

static void
window_info(FILE *fp)
{
    struct winmsg		*wp;
    struct io_msg		*mp;
    struct msg_list		list[2];
    static struct winmsg	winmsg;

    wp = NULL;
    mp = &_iom[fp - _iob];
    list[0].msg = mp;
    list[0].size = sizeof(struct io_msg);
    list[0].zero = 0;
    list[1].msg = &winmsg;
    list[1].size = sizeof(struct winmsg);
    list[1].zero = 0;

    mp->_cmd_stat = __WINDOW_INFO;
    sendm(mp->_admin_tid, 1, 2, list, list);
    if (mp->_cmd_stat == 0) {
	tcap_entry.term_num_rows = winmsg.prows;
	tcap_entry.term_num_cols = winmsg.pcols;
    }
}
