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
#define	CMDSZ	132

static	char		inbuf[CMDSZ];		/* command input buffer */
static	unsigned int	inpos = 0;		/* posn to put next input char */
static	unsigned int	inend = 0;		/* one past the last char */
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
    colposn[0] = 0;
    inpos = 1; inend = 1;
    colposn[1] = 1;
    update_cline(win, colposn[1]);
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
    unsigned		len;
    char *		stat; /* Pointer to status line text */

    if (kbdintr) {
	kbdintr = FALSE;
	if (!literal_next) imessage = TRUE;
	ch = CTRL('C');
    }

    /* Nuls are disallowed whether literal or not */
    if (ch == CTRL('@')) {
	beep(win);
	return(cmd_INCOMPLETE);
    }

    if (!literal_next) {
	switch (ch) {
	case CTRL('Q'):
	case CTRL('V'):
	    literal_next = TRUE;
	    /*
	     * Show a ^ (and we will leave the cursor on the ^).
	     * We will remove the ^ before inserting the literal char.
	     */
	    ch = '^';
	    break;

	case '\n':		/* end of line */
	case '\r':
	    inbuf[inend] = '\0';	/* terminate input line */
	    inpos = 0; inend = 0;
	    State = NORMAL;		/* return state to normal */
	    update_sline(win);		/* line is now a message line */
	    return(cmd_COMPLETE);	/* and indicate we are done */

	case '\b':		/* backspace or delete */
	case DEL:
	case CTRL('W'):		/* delete last word */
	    { int oldinpos = inpos; int i;
	      switch (ch) {
		case DEL:
		case '\b':
		    --inpos;
		    break;
		case CTRL('W'):
		{
		    int c;

		    while (inpos > 1 && (c = inbuf[inpos - 1], is_space(c)))
			--inpos;
		    while (inpos > 1 && (c = inbuf[inpos - 1], !is_space(c)))
			--inpos;
		}
	      }
	      /* Remember the number of screen characters deleted */
	      len = colposn[oldinpos] - colposn[inpos];
	      /* Delete the characters from the command line buffer */
	      memmove(inbuf+inpos, inbuf+oldinpos, inend-oldinpos);
	      memmove(colposn+inpos, colposn+oldinpos, inend-oldinpos+1);
	      inend -= (oldinpos - inpos);
	      /* Update the screen columns */
	      for (i=inpos; i <= inend; i++) colposn[i] -= len;
	      /* Move the end of the status line down to fill the gap */
              stat = &win->w_statusline.fxb_chars[win->w_statusline.fxb_rcnt];
	      memmove(stat+colposn[inpos], stat+colposn[inpos]+len,
		      colposn[inend]-colposn[inpos]);
	    }
	    if (inpos == 0) {
		/*
		 * Deleted past first char;
		 * go back to normal mode.
		 */
		State = NORMAL;
		return(cmd_CANCEL);
	    }
	    len = colposn[inend];
	    while (flexlen(&win->w_statusline) > len)
		flexrmchar(&win->w_statusline);
	    update_cline(win,colposn[inpos]);
	    return(cmd_INCOMPLETE);

	case '\t':
	{
	    char	*to_expand;
	    char	*expansion;

	    /*
	     * Find the word to be expanded.
	     */
	    inbuf[inend] = '\0';	/* ensure word is terminated */
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
		inend = inpos = to_expand - inbuf - 1;
		len = colposn[inpos - 1] + 1;
		while (flexlen(&win->w_statusline) > len)
		    flexrmchar(&win->w_statusline);
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
	    inpos = 1; inend = 1;
	    flexclear(&win->w_statusline);
	    (void) flexaddch(&win->w_statusline, inbuf[0]);
	    update_cline(win, colposn[inpos]);
	    return(cmd_INCOMPLETE);

	case CTRL('C'):
	case ESC:
	    inpos = 0; inend = 0;
	    flexclear(&win->w_statusline);
	    update_cline(win, colposn[inpos]);
	    State = NORMAL;
	    return(cmd_CANCEL);

	/* Simple line editing */

	case K_LARROW:
	    if (inpos > 1) {
		--inpos;
	        update_cline(win, colposn[inpos]);
	    }
	    else beep(win);
	    return(cmd_INCOMPLETE);

	case K_RARROW:
	    if (inpos < inend) {
		++inpos;
	        update_cline(win, colposn[inpos]);
	    }
	    else beep(win);
	    return(cmd_INCOMPLETE);

	default:
	    break;
	}
    } else {
	/*
	 * Insert a literal character
	 */
	register int i;
	register unsigned char *cp;

	/*
	 * Get rid of the ^ that we inserted, which is the char before inpos.
	 */
	inpos--; inend--;
	memmove(inbuf+inpos, inbuf+inpos+1, inend-inpos);
#if 0
	memmove(colposn+inpos, colposn+inpos+1, (inend-inpos)+1);
	for (i=inpos; i <= inend; i++) colposn[i]--;
#else
	/* The above two lines, performed in one loop */
	for (i=inpos, cp=&colposn[i]; i <= inend; i++, cp++) {
	    *cp = cp[1]-1;
	}
#endif
	/* Move the rest of the status line down */
	stat = &win->w_statusline.fxb_chars[win->w_statusline.fxb_rcnt];
	memmove(stat+colposn[inpos], stat+colposn[inpos]+1,
		colposn[inend]-colposn[inpos]);
	/* and remove its last character */
	flexrmchar(&win->w_statusline);

	literal_next = FALSE;
    }

    /*
     * Insert the character.
     */
    {
	unsigned	endposn;
	unsigned	w;
	char		*p;

	endposn = colposn[inend];
	w = vischar(ch, &p, -1);
	if (inend >= sizeof(inbuf) - 1
	    || endposn + w > win->w_ncols - 1) {
	    beep(win);
	} else {
	    int i;
	    memmove(inbuf+inpos+1, inbuf+inpos, inend-inpos);
	    memmove(colposn+inpos+1, colposn+inpos, inend-inpos+1);
	    for (i=inpos+1; i <= inend+1; i++) colposn[i] += w;
	    inend++; inbuf[inpos++] = ch;
	    colposn[inpos] = colposn[inpos-1] + w;

	    (void) lformat(&win->w_statusline, "%s", p);
	    /* That appended the representation of the char to the
	     * status line, extending the flexbuf, but we were supposed
	     * to insert the new char, not append it, so move the rest
	     * of the status line up, then deposit the new char(s) in
	     * the hole that this leaves.
	     */
            stat = &win->w_statusline.fxb_chars[win->w_statusline.fxb_rcnt];
            memmove(stat+colposn[inpos],
		    stat+colposn[inpos-1],
		    colposn[inend-1]-colposn[inpos-1]+1);
	    memcpy(stat+colposn[inpos-1],p,w);

	    /*
	     * If we just displayed the ^ for a literal next character,
	     * the cursor should be shown on the ^.
	     */
	    update_cline(win, colposn[inpos] - literal_next);
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
