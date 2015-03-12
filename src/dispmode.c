/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    dispmode.c
* module function:
    Handle "display mode", used for g/re/p, :set all, etc.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

static char	*(*disp_func) P((void));
static int	disp_colwidth;
static int	disp_maxcol;
static bool_t	disp_listmode;

/*
 * Start off "display" mode. The "func" argument is a function pointer
 * which will be called to obtain each subsequent string to display.
 * The function returns NULL when no more lines are available.
 */
void
disp_init(win, func, colwidth, listmode)
Xviwin		*win;
char		*(*func) P((void));
int		colwidth;
bool_t		listmode;
{
    State = DISPLAY;
    disp_func = func;
    if (colwidth > win->w_ncols)
	colwidth = win->w_ncols;
    disp_colwidth = colwidth;
    disp_maxcol = (win->w_ncols / colwidth) * colwidth;
    disp_listmode = listmode;
    (void) disp_screen(win, '\0');
}

/*
 * Display text in glass-teletype mode, in approximately the style of
 * the more(1) program.
 *
 * If the return value from (*disp_func)() is NULL, it means we've got
 * to the end of the text to be displayed, so we wait for another key
 * before redisplaying our editing screen.
 */
bool_t
disp_screen(win, ch)
Xviwin	*win;
int	ch;
{
    int		row;	/* current screen row */
    int		col;	/* current screen column */
    static bool_t	finished = FALSE;
    VirtScr		*vs;

    vs = win->w_vs;

    if (finished || kbdintr) {
	/*
	 * Ensure that the window on the current buffer is
	 * in the right place; then update the whole window.
	 */
	move_window_to_cursor(win);
	redraw_all(win, TRUE);
	State = NORMAL;
	/*
	 * If they typed a colon, take it as valid input.
	 */
	if (finished && ch == ':') {
	    stuff(":");
	}
	finished = FALSE;
	if (kbdintr) {
	    imessage = TRUE;
	}
	return(TRUE);
    }

    xvClear(vs);

    for (col = 0; col < disp_maxcol; col += disp_colwidth) {
	for (row = 0; row < VSrows(vs) - 1; row++) {
	    static char	*line;
	    int		width;

	    if (line == NULL && (line = (*disp_func)()) == NULL) {
		/*
		 * We've got to the end.
		 */
		prompt("[Hit return to continue] ");
		finished = TRUE;
		return(FALSE);
	    }

	    for (width = 0; *line != '\0'; line++) {
		char		*p;
		unsigned	w;

		w = vischar(*line, &p, disp_listmode ? -1 : width);

		if ((width += w) <= disp_colwidth) {
		    VSwrite(vs, row, col + width - w, p);
		} else {
		    /*
		     * The line is too long, so we
		     * have to wrap around to the
		     * next screen line.
		     */
		    break;
		}
	    }

	    if (*line == '\0') {
		if (disp_listmode) {
		    /*
		     * In list mode, we have to
		     * display a '$' to show the
		     * end of a line.
		     */
		    if (width < disp_colwidth) {
			VSputc(vs, row, col + width, '$');
		    } else {
			/*
			 * We have to wrap it
			 * to the next screen
			 * line.
			 */
			continue;
		    }
		}
		/*
		 * If we're not in list mode, or we
		 * were able to display the '$', we've
		 * finished with this line.
		 */
		line = NULL;
	    }
	}
    }

    prompt("[More] ");

    return(FALSE);
}

/*
 * Display a prompt on the bottom line of the screen.
 */
void
prompt(message)
    char	*message;
{
    VirtScr	*vs;
    int	row;

    vs = curwin->w_vs;

    row = VSrows(vs) - 1;
    VSgoto(vs, row, 0);
    VSset_colour(vs, VSCstatuscolour);
    VSwrite(vs, row, 0, message);
    VSset_colour(vs, VSCcolour);
    VSgoto(vs, row, strlen(message));
    VSflush(vs);
}
