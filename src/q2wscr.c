/* Copyright (c) 1990,1991,1992 Chris and John Downey */

/***

* program name:
    xvi
* function:
    PD version of UNIX "vi" editor, with extensions.
* module name:
    qnx_wscr.c
* module function:
    QNX Windows/virtscr interface module.

    Note that this module assumes the C86 compiler,
    which is an ANSI compiler, rather than the standard
    QNX compiler, which is not.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

/*
 * QNX-specific include files.
 *
 * <stdio.h> etc. get included by "xvi.h".
 */
#include <io.h>
#include <dev.h>
#include <timer.h>
#include <taskmsgs.h>
#include <process.h>
#include <portio.h>
#include <tcapkeys.h>

#include "xvi.h"

#include <windows.h>
#include <font.h>

/*
 * This tells the editor whether we can handle subshell invocation.
 */
const	bool_t		subshells = FALSE;

#define	LEADING		10	/* space between lines in tips */

static	void		process_event(void);
static	VirtScr		*win_open(void);
static	void		set_font(void);
static	void		setup_lines(VirtScr *, int, int);
static	VirtScr		*scr_init(void);
static	int		key_translate(int c);

static	VirtScr		*newscr P((VirtScr *, genptr *));
static	void		closescr P((VirtScr *));
static	void		clear_all P((VirtScr *));
static	void		clear_rows P((VirtScr *, int, int));
static	void		clear_line P((VirtScr *, int, int));
static	void		xygoto P((VirtScr *, int, int));
static	void		xyadvise P((VirtScr *, int, int, int, char *));
static	void		put_str P((VirtScr *, int, int, char *));
static	void		put_char P((VirtScr *, int, int, int));
static	void		set_colour P((VirtScr *, int));
static	int		colour_cost P((VirtScr *));
static	int		scroll P((VirtScr *, int, int, int));
static	void		flushout P((VirtScr *));
static	void		pbeep P((VirtScr *));

typedef	struct	winline {
	char	*line;
	char	ntag[6];
	char	stag[6];
	int	dirty;
	int	inv_cols;		/* number of coloured columns */
	int	last_n_pos;		/* last value of start of normal text */
	int	inv_colour;
} WinLine;

static	WinLine		*wlines = NULL;
static	int		nlines = 0;

static	int		char_height;
static	int		char_base;
static	int		char_width;
static	int		extra_height = 0;
static	int		extra_width = 0;
static	int		invert_colour = 0;
static	int		curr_inv_colour = BLUE;

static	int		timer_port;

/*
 * This is static only for now - it will have to become dynamic later.
 */
static	VirtScr		*vs;

int
main(argc, argv)
int	argc;
char	*argv[];
{
    char	*env;

    timer_port = attach_port(0, 0);
    if (timer_port == 0) {
    	fprintf(stderr, "Cannot obtain timer port\n");
	exit(1);
    }

    vs = win_open();

    env = getenv("WXVINIT");
    if (env == NULL) {
    	env = getenv("XVINIT");
    }
    ignore_signals();
    vs->pv_window = (genptr *) xvi_startup(vs, argc, argv, env);
    catch_signals();

    while (1) {
	process_event();
    }
}

void
startup_error(str)
char	*str;
{
    (void) fputs(str, stderr);
}

static VirtScr *
win_open()
{
    static WIND_COLORS	colours;	/* all colours will be 0 at run time */
    static char		iconbuf[4000];
    RECT_AREA		region;
    VirtScr		*vs;
    int			ipid;
    char		lhs[2];
    char		rhs[3];
    int			i;

    vs = scr_init();
    if (vs == NULL) {
    	return(NULL);
    }

    SetName("Xvi", "editor");
    GraphicsOpen(NULL);

    ipid = Picture("icon", "/user/local/lib/xvi.icon.pict");
    if (ipid != 0) {
	CopyElements(NULL, iconbuf, sizeof(iconbuf), NULL);
	PictureClose(ipid);
    }

    set_font();
    Picture("Xvi", NULL);

    colours.bkgd = BLACK;
    WindowColors(&colours);
    SetFill("Ti", BLACK, SOLID_PAT);
    SetColor("T", OFF_WHITE);

    setup_lines(vs, 25, 80);

    WindowOpen("Xvi",
		    vs->pv_rows * char_height + V_TPP,
		    vs->pv_cols * char_width,
		    NULL, "RtxX", "Xvi", 0);
    PaneChange(0, 0, 0, "S");

    if (ipid != 0) {
	WindowIcon(NULL, NULL, "-l", iconbuf, 0);
    }

    /*
     * Calculate offsets used in size of region for later use.
     */
    (void) WindowInfo(NULL, NULL, &region, NULL, NULL, NULL);
    extra_height = region.height - (vs->pv_rows * char_height);
    extra_width = region.width - (vs->pv_cols * char_width);

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

    return(vs);
}

static void
process_event()
{
    xvEvent	event;
    xvResponse	*resp;
    EVENT_MSG	msg;
    static long	timeout = 0;
    static int	do_expose = 0;

    /*
     * Set a timeout on input if asked to do so.
     */
    if (timeout != 0) {
	/*
	 * Timeout value is in milliseconds, so we have to convert
	 * to 50 millisecond ticks, rounding up as we do it.
	 */
	(void) set_timer(TIMER_SIGNAL_PORT, RELATIVE,
			(int) ((timeout + 19) / 20), timer_port, NULL);
    }

    flushout(vs);

    event.ev_vs = vs;
    if (do_expose && !EventWaiting()) {
	event.ev_type = Ev_refresh;
	event.ev_do_clear = FALSE;
	do_expose = FALSE;
    } else if (GetEvent(0, (void *) &msg, sizeof(msg)) == (timer_port << 8)) {
	event.ev_type = Ev_timeout;
    } else {
	if (timeout) {
	    set_timer(TIMER_CANCEL, RELATIVE, 0, 0, NULL);
	}

	/*
	 * Assume that if we were interrupted, no event has been received,
	 * so we can simply process the interrupt and then continue.
	 */
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

	    /*
	     * Not interrupted; must have been an event.
	     */
	    switch (Event(&msg)) {
	    case TYPED:
		event.ev_type = Ev_char;
		if (msg.hdr.code & 0xff00) {
		    event.ev_inchar = key_translate(msg.hdr.code & 0xff);
		} else {
		    event.ev_inchar = msg.hdr.code;
		}
		break;

	    case RESIZED:
		switch (msg.hdr.code) {
		case 'F':		/* expanded to full screen size */
		case 'N':		/* returned to normal size */
		case 'M':		/* minimized */
		case 0:		/* resized directly */
		{
		    RECT_AREA	position;
		    RECT_AREA	region;
		    int		old_rows, old_cols;
		    int		new_rows, new_cols;

		    /*
		     * Get the old and new window sizes.
		     */
		    old_rows = vs->pv_rows;
		    old_cols = vs->pv_cols;
		    (void) WindowInfo(NULL, &position, &region, NULL, NULL, NULL);
		    new_rows = (region.height - extra_height) / char_height;
		    new_cols = (region.width - extra_width) / char_width;
		    if (old_rows == new_rows && old_cols == new_cols) {
			return;
		    }
		    if (new_rows < 4) {
			new_rows = 4;
		    }
		    if (new_cols < 20) {
			new_cols = 20;
		    }

    #if 0
		    (void) printf("code=%c height=%d, width=%d, rows=%d, cols=%d\n",
				    msg.hdr.code, region.height, region.width,
				    new_rows, new_cols);
    #endif

		    /*
		     * Round the window size down to an integer number of
		     * rows and columns; change the actual size on screen.
		     */
		    position.height = (new_rows * char_height) + extra_height;
		    position.width = (new_cols * char_width) + extra_width;
		    WindowChange(&position, 0, 0, KEEP, KEEP);

		    setup_lines(vs, new_rows, new_cols);

		    event.ev_type = Ev_resize;
		    event.ev_rows = new_rows - old_rows;
		    event.ev_columns = new_cols - old_cols;
		    break;
		}

		case 'I':		/* iconified */
		case 'X':		/* de-iconified */
		    return;

		default:
		    EventNotice("Unrecognised resize event", &msg);
		    return;
		}
		break;

	    case EXPOSED:
		do_expose = TRUE;
		return;

	    case CLICK:
		if (msg.hdr.code != 'S') {
		    return;
		}
		event.ev_type = Ev_mouseclick;
		event.ev_m_row = msg.hdr.row / char_height;
		event.ev_m_col = msg.hdr.col / char_width;
		break;

	    case QUIT:
	    case CLOSED:
	    default:
		EventNotice("Unknown event", &msg);
		return;
	    }
	}
    }

    resp = xvi_handle_event(&event);
    if (resp->xvr_type == Xvr_exit) {
	sys_exit(0);
    }
    timeout = resp->xvr_timeout;
}

static VirtScr *
newscr(scr, win)
VirtScr	*scr;
genptr	*win;
{
    return(win_open());
}

/*ARGSUSED*/
static void
closescr(scr)
VirtScr	*scr;
{
}

/*ARGSUSED*/
static void
clear_all(vs)
VirtScr	*vs;
{
    clear_rows(vs, 0, vs->pv_rows - 1);
}

/*ARGSUSED*/
static void
clear_rows(scr, start, end)
VirtScr	*scr;
int	start;
int	end;
{
    int	row;

    for (row = start; row <= end; row++) {
	(void) memset(wlines[row].line, ' ', scr->pv_cols);
	wlines[row].dirty = 1;
	wlines[row].inv_cols = 0;
	wlines[row].inv_colour = curr_inv_colour;
    }
}

/*ARGSUSED*/
static void
clear_line(scr, row, col)
VirtScr	*scr;
int	row;
int	col;
{
    (void) memset(wlines[row].line + col, ' ', scr->pv_cols - col);
    wlines[row].dirty = 1;
    if (col < wlines[row].inv_cols) {
    	wlines[row].inv_cols = col;
	wlines[row].inv_colour = curr_inv_colour;
    }
}

static void
xygoto(vs, r, c)
VirtScr	*vs;
int	r, c;
{
    Erase("cursor");
    SetFill("Ti", RED, SOLID_PAT);
    DrawAt((r + 0) * char_height + char_base + V_TPP, c * char_width);
    DrawText(&(wlines[r].line[c]), 0, 0, 1, NULL, "cursor");
    SetFill("Ti", BLACK, SOLID_PAT);
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
}

/*ARGSUSED*/
static void
put_str(scr, row, col, text)
VirtScr	*scr;
int	row;
int	col;
char	*text;
{
    int	i;

    for (i = 0; text[i] != '\0'; i++) {
    	wlines[row].line[col + i] = text[i];
    }
    wlines[row].dirty = 1;

    if (invert_colour && (col + i + 1) > wlines[row].inv_cols) {
    	wlines[row].inv_cols = col + i + 1;
	wlines[row].inv_colour = curr_inv_colour;
    } else if (!invert_colour && col < wlines[row].inv_cols) {
    	wlines[row].inv_cols = col;
    }
}

/*ARGSUSED*/
static void
put_char(scr, row, col, c)
VirtScr	*scr;
int	row;
int	col;
int	c;
{
    wlines[row].line[col] = c;
    wlines[row].dirty = 1;

    if (invert_colour && (col + 1) > wlines[row].inv_cols) {
    	wlines[row].inv_cols = col + 1;
	wlines[row].inv_colour = curr_inv_colour;
    } else if (!invert_colour && col < wlines[row].inv_cols) {
    	wlines[row].inv_cols = col;
    }
}

static void
set_colour(vs, c)
VirtScr	*vs;
int	c;
{
    int		inverse;

    inverse = (c & 0x0004);
    if (inverse != invert_colour) {
	invert_colour = inverse;
    }
    if (inverse) {
	if (c == vs->pv_colours[VSCstatuscolour]) {
	    curr_inv_colour = BLUE;
	} else {
	    curr_inv_colour = RED;
	}
    }
}

/*ARGSUSED*/
static int
colour_cost(scr)
VirtScr	*scr;
{
    return(0);
}

/*
 * Scroll an area of the window by nlines.
 * If nlines > 0, scrolling is "up", otherwise "down".
 */
static int
scroll(vs, start, end, nlines)
VirtScr	*vs;
int	start, end, nlines;
{
    int		i;
    int		row;

    return(0);

#if 1
    if (nlines > 0) {
	for (row = start; row <= end - nlines; row++) {
	    Erase(wlines[row].ntag);
	}
	for (row = start; row <= end - nlines; row++) {
	    ShiftBy(wlines[row + nlines].ntag, - nlines * char_height, 0);
	    ChangeTag(wlines[row + nlines].ntag, wlines[row].ntag);
	}
	clear_rows(vs, start + nlines, end);
    } else {
	nlines = -nlines;
	for (row = end; row >= start + nlines; --row) {
	    Erase(wlines[row].ntag);
	}
	for (row = end; row >= start + nlines; --row) {
	    ShiftBy(wlines[row - nlines].ntag, nlines * char_height, 0);
	    ChangeTag(wlines[row - nlines].ntag, wlines[row].ntag);
	}
	clear_rows(vs, start, start + nlines - 1);
    }

    return(1);
#endif
}

/*ARGSUSED*/
static void
flushout(vs)
VirtScr	*vs;
{
    register int	row;

    for (row = 0; row < vs->pv_rows; row++) {
	if (wlines[row].dirty == 0) {
		continue;
	}
	wlines[row].dirty = 0;
	if (wlines[row].inv_cols > 0) {
	    DrawAt((row + 0) * char_height + char_base + V_TPP, 0);
	    SetFill("T", wlines[row].inv_colour, SOLID_PAT);
	    DrawText(wlines[row].line, 0, 0, wlines[row].inv_cols, "r",
						    wlines[row].stag);
	    SetFill("T", BLACK, SOLID_PAT);
	} else {
	    Erase(wlines[row].stag);
	}

	if (wlines[row].last_n_pos == 0) {
	    ChangeText(wlines[row].ntag, wlines[row].line, 0, vs->pv_cols, 0);
	} else {
	    /*Erase(wlines[row].ntag);*/
	    DrawAt((row + 0) * char_height + char_base + V_TPP,
			wlines[row].inv_cols * char_width);
	    DrawText(wlines[row].line + wlines[row].inv_cols, 0, 0,
			vs->pv_cols - wlines[row].inv_cols,
			"r", wlines[row].ntag);
	    wlines[row].last_n_pos = wlines[row].inv_cols;
	}
    }
    (void) Draw();
}

/*ARGSUSED*/
static void
pbeep(vs)
VirtScr	*vs;
{
    /*
     * There is no way to ask the QNX Windows server to beep.
     * Have to put in support for flashing windows later.
     */
}

static void
set_font()
{
    int		fontid;

    fontid = SetFont("T", NULL, NULL, 0, 130);
    char_height = ((CHARH + LEADING + V_TPP - 1) / V_TPP) * V_TPP;
    char_base = ((CHARB + LEADING + V_TPP - 1) / V_TPP) * V_TPP;
    char_width = 66;
#if 0
    if (fontid > 0) {
	REAL_EXTENT	size;
	int		base;

	ScreenFontInfo(fontid, NULL, NULL, &size, &base, NULL, NULL);
	printf("id=%d\n", fontid);
	printf("h=%d, w=%d, b=%d\n", size.h, size.w, base);
	printf("V_TPP=%d, H_TPP=%d\n", V_TPP, H_TPP);
	char_height = (size.h * V_TPP) / 8;
	char_width = (size.w * H_TPP) / 8;
	char_base = (base * V_TPP) / 8;
	printf("ch=%d, cw=%d, cb=%d\n", char_height, char_width, char_base);
    } else {
	char_height = CHARH;
	char_width = CHARW;
	char_base = CHARB;
    }
#endif
}

static void
setup_lines(vs, nrows, ncols)
VirtScr	*vs;
int	nrows;
int	ncols;
{
    register int	row;

    if (wlines == NULL) {
	nlines = nrows;
	wlines = (WinLine *) malloc(nlines * sizeof(*wlines));
    }
    if (wlines == NULL) {
	(void) fprintf(stderr, "Out of memory\n");
	exit(1);
    }

    /*
     * Ensure that the tags for the existing set of rows are
     * correctly ordered; otherwise we might create one of the
     * new lines with an existing tag.
     */
    if (nrows > vs->pv_rows) {
	for (row = 0; row < vs->pv_rows; row++) {
	    char	buf[4];

	    (void) strncpy(buf, wlines[row].ntag, sizeof(buf));
	    (void) sprintf(wlines[row].ntag, "%-3.3d", row);
	    (void) ChangeTag(buf, wlines[row].ntag);

	    (void) strncpy(buf, wlines[row].stag, sizeof(buf));
	    (void) sprintf(wlines[row].stag, "S%-3.3d", row);
	    (void) ChangeTag(buf, wlines[row].stag);
	}
    }

    /*
     * Free up unused space, and remove any drawn elements from the window.
     */
    for (row = nrows; row < vs->pv_rows; row++) {
	free(wlines[row].line);
	Erase(wlines[row].ntag);
	Erase(wlines[row].stag);
    }
    if (nlines != nrows) {
	nlines = nrows;
	wlines = realloc(wlines, nlines * sizeof(*wlines));
	if (wlines == NULL) {
	    goto nomem;
	}
    }

    /*
     * Allocate new space for each new row.
     */
    for (row = vs->pv_rows; row < nrows; row++) {
	wlines[row].line = malloc((unsigned) ncols + 1);
	if (wlines[row].line == NULL) {
	    goto nomem;
	}
	(void) memset(wlines[row].line, ' ', ncols);
	wlines[row].line[ncols] = '\0';
	(void) sprintf(wlines[row].ntag, "%-3.3d", row);
	(void) sprintf(wlines[row].stag, "S%-3.3d", row);
	wlines[row].dirty = 0;
	wlines[row].inv_cols = 0;
	wlines[row].last_n_pos = 999;
    }

    /*
     * Reallocate line buffers for existing rows.
     */
    for (row = 0; row < vs->pv_rows && row < nrows; row++) {
	wlines[row].line = realloc(wlines[row].line, (unsigned) ncols + 1);
	if (wlines[row].line == NULL) {
	    goto nomem;
	}
	(void) memset(wlines[row].line, ' ', ncols);
	wlines[row].line[ncols] = '\0';
	wlines[row].last_n_pos = 999;
    }

    vs->pv_rows = nrows;
    vs->pv_cols = ncols;

    /*
    for (row = 0; row < vs->pv_rows; row++) {
	wlines[row].inv_cols = 0;
	wlines[row].last_n_pos = 999;
    }
    Erase(ALL);
    */

    return;

nomem:
    Tell("Internal error", "Out of memory");
    (void) exPreserveAllBuffers();
    exit(1);
}

/*
 * Obtain a new VirtScr structure, and initialise all the
 * function pointer elements to the appropriate functions.
 * Returns NULL only if we cannot allocate the space.
 */
static VirtScr *
scr_init(void)
{
    VirtScr	*vs;

    vs = (VirtScr *) malloc(sizeof(VirtScr));
    if (vs == NULL) {
    	return(NULL);
    }

    vs->pv_window = NULL;
    vs->pv_rows = 0;
    vs->pv_cols = 0;
    vs->v_open = newscr;
    vs->v_close = closescr;
    vs->v_clear_all = clear_all;
    vs->v_clear_rows = clear_rows;
    vs->v_clear_line = clear_line;
    vs->v_goto = xygoto;
    vs->v_advise = xyadvise;
    vs->v_write = put_str;
    vs->v_putc = put_char;
    vs->v_set_colour = set_colour;
    vs->v_colour_cost = colour_cost;
    vs->v_decode_colour = xv_decode_colour;
    vs->v_flush = flushout;
    vs->v_beep = pbeep;
    vs->v_insert = NULL;
    vs->v_scroll = scroll;
    vs->v_flash = NULL;
    vs->v_can_scroll = NULL;

    return(vs);
}

void
tty_startv()
{
}

void
tty_endv()
{
}

static int
key_translate(c)
int	c;
{
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
    return(c);
}
