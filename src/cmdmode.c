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
 * This is the increment for resizing the commandline.
 * If they have an 80-column screen and never overflow it,
 * they'll never need more.
 */
#define	CMDSZCHUNK	80

static	int		cmdsz = 0;	/* size of buffers */
static	char		*inbuf = NULL;	/* command input buffer */
static	unsigned int	inpos = 0;	/* posn to put next input char */
static	unsigned int	inend = 0;	/* one past the last char */
static	unsigned short 	*colposn = NULL;/* holds n chars per char */

static bool_t
cmd_buf_alloc()
{
    int new_cmdsz;
    char *new_inbuf;
    unsigned short *new_colposn;

    new_cmdsz = (cmdsz == 0) ? CMDSZCHUNK : cmdsz + CMDSZCHUNK;

    if ((new_inbuf = re_alloc(inbuf, new_cmdsz)) == NULL) {
	show_error("Failed to allocate command line inbuf");
	return FALSE;
    }
    if ((new_colposn = re_alloc(colposn, new_cmdsz * sizeof(*colposn))) == NULL) {
	free(new_inbuf);
	return FALSE;
    }

    cmdsz = new_cmdsz;
    inbuf = new_inbuf;
    colposn = new_colposn;

    return TRUE;
}

/*
 * cmd_init(firstch)
 *
 * Initialise command line input.
 */
void
cmd_init(firstch)
int	firstch;
{
    if (inpos > 0) {
	show_error("Internal error: re-entered command line input mode");
	return;
    }

    inend = 0;

    if (!cmd_buf_alloc())
	return;

    State = CMDLINE;

    flexclear(&curwin->w_statusline);
    (void) flexaddch(&curwin->w_statusline, firstch);
    inbuf[0] = firstch;
    inbuf[1] = '\0';
    colposn[0] = 0;
    inpos = 1; inend = 1;
    colposn[1] = 1;
    update_cline();
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
 * cmd_input(character)
 *
 * Deal with command line input. Takes an input character and returns
 * one of cmd_CANCEL (meaning they deleted past the prompt character),
 * cmd_COMPLETE (indicating that the command has been completely input),
 * or cmd_INCOMPLETE (indicating that command line is still the right
 * mode to be in).
 *
 * Once cmd_COMPLETE has been returned, it is possible to call
 * get_cmd() to obtain the command line.
 */
Cmd_State
cmd_input(ch)
int	ch;
{
    unsigned		len;
    unsigned		w;

    if (kbdintr) {
	kbdintr = FALSE;
	if (!literal_next) {
	    imessage = TRUE;
	    goto case_kbdintr_ch;
	} else {
	    ch = kbdintr_ch;
	}
    }

    /* Nuls are disallowed whether literal or not */
    if (ch == CTRL('@')) {
	beep();
	return(cmd_INCOMPLETE);
    }

    if ((inend >= (cmdsz - CMDSZCHUNK/2)) && !cmd_buf_alloc()) {
	/* Memory allocation failure. Free up what we can, and reset
	* things to a semi sane state. Maybe the user will be able
	* to save the file buffers...
	*/
	free(inbuf);
	free(colposn);
	cmdsz = 0; inpos = 0; inend = 0;
	State = NORMAL;
	return cmd_CANCEL;
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
	    return(cmd_COMPLETE);	/* and indicate we are done */

	case '\b':		/* backspace or delete */
	case DEL:
	case K_DELETE:		/* delete character key */
	case CTRL('W'):		/* delete last word */
	    { int oldinpos = inpos; int i;
	      switch (ch) {
		case DEL:
		case '\b':
		    --inpos;
		    break;
		case K_DELETE:
		    /* Delete the character under the cursor */
		    if (inpos < inend) {
			oldinpos++;
		    }
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
	      memmove(colposn+inpos, colposn+oldinpos,
		      (inend-oldinpos+1) * sizeof(*colposn));
	      inend -= (oldinpos - inpos);
	      /* Update the screen columns */
	      for (i=inpos; i <= inend; i++) colposn[i] -= len;
	      flexrm(&curwin->w_statusline, colposn[inpos], len);
	    }
	    if (inpos == 0) {
		/*
		 * Deleted past first char;
		 * go back to normal mode.
		 */
		State = NORMAL;
		return(cmd_CANCEL);
	    }
	    inbuf[inend] = '\0';
	    update_cline();
	    return(cmd_INCOMPLETE);

	case '\t':
	{
	    char	*to_expand;
	    char	*expansion;

	    inbuf[inend] = '\0';	/* ensure word is terminated */

	    /* Only do filename completion for commands that want it. */
	    if (!should_fexpand(inbuf)) {
		break;
	    }

	    /*
	     * Find the word to be expanded.
	     */
	    to_expand = strrchr(inbuf, ' ');
	    if (to_expand == NULL || *(to_expand + 1) == '\0') {
		beep();
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
		 * when something like "*.h<TAB>" is typed.
		 */
		inpos = to_expand - inbuf - 1;
		len = colposn[inend] - colposn[inpos];
		flexrm(&curwin->w_statusline, colposn[inpos], len);
		inend = inpos;
	        inbuf[inend] = '\0';
		if (common_prefix(expansion) > 1)
		    beep();
		stuff(" %s", expansion);
	    } else {
		beep();
	    }

	    return(cmd_INCOMPLETE);
	}

	case EOF:
	case CTRL('U'):		/* line kill */
	    inpos = 1; inend = 1;
	    inbuf[inend] = '\0';
	    /* Just leave the first character (:/?!) */
	    curwin->w_statusline.fxb_wcnt = 1;
	    update_cline();
	    return(cmd_INCOMPLETE);

	case_kbdintr_ch:	/* a label! */
	case ESC:
	    inpos = 0; inend = 0;
	    inbuf[inend] = '\0';
	    State = NORMAL;
	    update_cline();
	    return(cmd_CANCEL);

	/* Simple line editing */

	case K_LARROW:
	    if (inpos > 1) {
		--inpos;
	        update_cline();
	    }
	    else beep();
	    return(cmd_INCOMPLETE);

	case K_RARROW:
	    if (inpos < inend) {
		++inpos;
	        update_cline();
	    }
	    else beep();
	    return(cmd_INCOMPLETE);

	case K_HOME:
	    inpos = 1;
	    update_cline();
	    return(cmd_INCOMPLETE);

	case K_END:
	    inpos = inend;
	    update_cline();
	    return(cmd_INCOMPLETE);

	/*
	 * Function key F1 isn't used to give help on the cmdline
	 * so map it to "#1". This means that if they have a "Help" key
	 * it will insert "#1" if pressed on the command line.
	 */
	case K_HELP:
	    stuff_to_map("#1");
	    return(cmd_INCOMPLETE);

	/* Ignore other special keys in command-line mode */
	case K_UNDO:
	case K_INSERT:
	case K_UARROW:
	case K_DARROW:
	case K_CGRAVE:
	case K_PGDOWN:
	case K_PGUP:
	    beep();
	    return(cmd_INCOMPLETE);

	default:
	    break;
	}
    } else {
	/*
	 * Insert a literal character
	 */
	register int i;
	register unsigned short *cp;

	/*
	 * Get rid of the ^ that we inserted, which is the char before inpos.
	 */
	inpos--; inend--;
	memmove(inbuf+inpos, inbuf+inpos+1, inend-inpos);
#if 0
	memmove(colposn+inpos, colposn+inpos+1,
		(inend-inpos+1)*sizeof(*colposn));
	for (i=inpos; i <= inend; i++) colposn[i]--;
#else
	/* The above two lines, performed in one loop */
	for (i=inpos, cp=&colposn[i]; i <= inend; i++, cp++) {
	    *cp = cp[1]-1;
	}
#endif
	/* Move the rest of the status line down */
	flexrm(&curwin->w_statusline, colposn[inpos], 1);

	literal_next = FALSE;
    }

    /*
     * Insert the character.
     */
    {
	char	*p;
	int	i;

	w = vischar(ch, &p, -1);

	if (inend >= cmdsz && !cmd_buf_alloc()) {
	    /*
	     * Memory allocation failure. Free up what we can and reset
	     * things to a semi sane state. Maybe the user will be able
	     * to save the file buffers...
	     */
	    free(inbuf);
	    free(colposn);
	    cmdsz = 0; inpos = 0; inend = 0;
	    inbuf[inend] = '\0';
	    State = NORMAL;
	    return cmd_CANCEL;
	}

	/* Add the displayed version to the status line text */
	flexinsstr(&curwin->w_statusline, colposn[inpos], p);

	/*
	 * Move the rest of the command line up by one character position.
	 * If inpos == inend (cursor at tne of line) this only moves the
	 * terminating \0
	 */
	inend++;
	memmove(inbuf+inpos+1, inbuf+inpos, inend-inpos);
	for (i = inend; i > inpos; i--) {
	    colposn[i] = colposn[i-1] + w;
	}
	inbuf[inpos++] = ch;

	update_cline();
    }

    return(cmd_INCOMPLETE);
}

char *
get_cmd()
{
    return(inbuf);
}

/*
 * Which screen column should the cursor be displayed in?
 *
 * Unlike the normal display, if the cursor is on a wide character like ^X,
 * it should be displayed on the leftmost character because the command line
 * is always effectively in INSERT mode.
 *
 * If we just displayed the ^ for a literal next character,
 * the cursor should be shown on the ^, not after it.
 */
int
get_pos()
{
    return(colposn[inpos] - literal_next);
}
