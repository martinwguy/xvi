/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    cmdmode.c
* module function:
    Handle "command mode", used for input on the command line.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

/*
 * Size of command buffer - we won't allow anything more
 * to be typed when we get to this limit.
 */
#define	CMDSZ	80

static	char		inbuf[CMDSZ];		/* command input buffer */
static	unsigned int	inpos = 0;		/* posn of next input char */
static	unsigned char	colposn[CMDSZ];		/* holds n chars per char */

/*
 * cmd_init(window, firstch)
 *
 * Initialise command line input.
 */
void
cmd_init(win, firstch)
Xviwin	*win;
int	firstch;
{
    if (inpos > 0) {
	show_error(win, "Internal error: re-entered command line input mode");
	return;
    }

    State = CMDLINE;

    flexclear(&win->w_statusline);
    (void) flexaddch(&win->w_statusline, firstch);
    inbuf[0] = firstch;
    inpos = 1;
    update_cline(win);
    colposn[0] = 0;
}

/*
 * Given a list of names in a space-separated string, find the longest
 * prefix that they all have in common; in the trivial case where only
 * one name is given, this is obviously equivalent to the whole name.
 * Return the number of names in the string; the common prefix is
 * marked simply by setting the character following it to '\0'.
 */
static int
common_prefix(s)
    char * s;
{
    int argc;
    char ** argv;

    makeargv(s, &argc, &argv, " ");
    if (argc > 1)
    {
	int i;

	for (i = 1; i < argc; i++)
	{
	    register char * p0;
	    register char * p1;
	    register int c;

	    p0 = s;
	    p1 = argv[i];
	    while ((c = *p0) == *p1++ && c != '\0')
		p0++;
	    *p0 = '\0';
	}
    }
    free(argv);
    return argc;
}

/*
 * cmd_input(window, character)
 *
 * Deal with command line input. Takes an input character and returns
 * one of cmd_CANCEL (meaning they deleted past the prompt character),
 * cmd_COMPLETE (indicating that the command has been completely input),
 * or cmd_INCOMPLETE (indicating that command line is still the right
 * mode to be in).
 *
 * Once cmd_COMPLETE has been returned, it is possible to call
 * get_cmd(win) to obtain the command line.
 */
Cmd_State
cmd_input(win, ch)
Xviwin	*win;
int	ch;
{
    static bool_t	literal_next = FALSE;
    unsigned		len;

    if (!literal_next) {
	switch (ch) {
	case CTRL('V'):
	    literal_next = TRUE;
	    return(cmd_INCOMPLETE);

	case '\n':		/* end of line */
	case '\r':
	    inbuf[inpos] = '\0';	/* terminate input line */
	    inpos = 0;
	    State = NORMAL;		/* return state to normal */
	    update_sline(win);		/* line is now a message line */
	    return(cmd_COMPLETE);	/* and indicate we are done */

	case '\b':		/* backspace or delete */
	case DEL:
	case CTRL('W'):		/* delete last word */
	    switch (ch) {
		case '\b':
		case DEL:
		    ch = '\b';
		    inbuf[--inpos] = '\0';
		    break;
		case CTRL('W'):
		{
		    int c;

		    while (inpos > 1 && (c = inbuf[inpos - 1], is_space(c)))
			--inpos;
		    while (inpos > 1 && (c = inbuf[inpos - 1], !is_space(c)))
			--inpos;
		    inbuf[inpos] = '\0';
		}
	    }
	    if (inpos == 0) {
		/*
		 * Deleted past first char;
		 * go back to normal mode.
		 */
		State = NORMAL;
		return(cmd_CANCEL);
	    }
	    len = colposn[inpos - 1] + 1;
	    while (flexlen(&win->w_statusline) > len)
		(void) flexrmchar(&win->w_statusline);
	    update_cline(win);
	    return(cmd_INCOMPLETE);

	case ESC:
	{
	    char	*to_expand;
	    char	*expansion;

	    /*
	     * Find the word to be expanded.
	     */
	    inbuf[inpos] = '\0';	/* ensure word is terminated */
	    to_expand = strrchr(inbuf, ' ');
	    if (to_expand == NULL || *(to_expand + 1) == '\0') {
	    	beep(win);
		return(cmd_INCOMPLETE);
	    } else {
		to_expand++;
	    }

	    /*
	     * Expand the word.
	     */
	    expansion = fexpand(to_expand, TRUE);
	    if (*expansion != '\0') {
		/*
		 * Expanded okay - remove the original and stuff
		 * the expansion into the input stream. Note that
		 * we remove the preceding space character as well;
		 * this avoids problems updating the command line
		 * when something like "*.h<ESC>" is typed.
		 */
		inpos = to_expand - inbuf - 1;
		len = colposn[inpos - 1] + 1;
		while (flexlen(&win->w_statusline) > len)
		    (void) flexrmchar(&win->w_statusline);
		if (common_prefix(expansion) > 1)
		    beep(win);
		stuff(" %s", expansion);
	    } else {
		beep(win);
	    }

	    return(cmd_INCOMPLETE);
	}

	case EOF:
	case CTRL('U'):		/* line kill */
	    inpos = 1;
	    inbuf[1] = '\0';
	    flexclear(&win->w_statusline);
	    (void) flexaddch(&win->w_statusline, inbuf[0]);
	    update_cline(win);
	    return(cmd_INCOMPLETE);

	default:
	    break;
	}
    }

    literal_next = FALSE;

    if (inpos >= sizeof(inbuf) - 1) {
	/*
	 * Must not overflow buffer.
	 */
	beep(win);
    } else {
	unsigned	curposn;
	unsigned	w;
	char		*p;

	curposn = colposn[inpos - 1];
	w = vischar(ch, &p, (int) curposn);
	if (curposn + w >= win->w_ncols - 1) {
	    beep(win);
	} else {
	    colposn[inpos] = curposn + w;
	    inbuf[inpos++] = ch;
	    (void) lformat(&win->w_statusline, "%s", p);
	    update_cline(win);
	}
    }

    return(cmd_INCOMPLETE);
}

/*ARGSUSED*/
char *
get_cmd(win)
Xviwin	*win;
{
    return(inbuf);
}

