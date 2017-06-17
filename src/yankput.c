/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    yankput.c
* module function:
    Functions to handle "yank" and "put" commands.

    Note that there is still some code in normal.c to do
    some of the work - this will have to be changed later.

    Some of the routines and data structures herein assume ASCII
    order, so I don't know if they are particularly portable.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

/*
 * Structure to store yanked text or yanked lines.
 */
typedef struct yankbuffer {
    enum {
	y_none,
	y_chars,
	y_lines
    } y_type;

    /* For char-based yanks, the values of (1st_text, 2nd_text, line_buf) can be:
     * - Yank from a single line: (something, NULL, NULL)
     * - Yank from a two consecutive lines: (something, something, NULL)
     * - Yank from three or more lines: (something, something, something)
     *
     * For line-based yanks, 1st and 2nd are not used and y_line_buf is always set.
     */
    char	*y_1st_text;
    char	*y_2nd_text;
    Line	*y_line_buf;
} Yankbuffer;

/*
 * For named buffers, we have an array of Yankbuffer structures,
 * mapped by printable ascii characters. Only the alphabetic
 * characters, and '@', should be directly settable by the user.
 * Uppercase buffer names mean "append" rather than "replace" when yanking.
 *
 * POSIX mandates buffers a-z (==A-Z), 1-9, and the unnamed buffer.
 * xvi uses these as well as:
 * /	The last forward search command line
 * ?	The last backward search command lines
 * :	The last ex command line
 * !	The last shell command line
 * @	The unnamed buffer
 * <	The last inserted text
 *
 * We exploit ASCII, using the range '1' to 'Z' and mapping the out-of-range
 * characters ('!' and '/') to unused chars in the hole between '9' and '@'.
 */
#define	LOWEST_NAME	'1'
#define	HIGHEST_NAME	'Z'
#define	NBUFS		(HIGHEST_NAME - LOWEST_NAME + 1)

static	Yankbuffer	yb[NBUFS];

static	void		put P((char *, bool_t, bool_t, bool_t));
static	Yankbuffer	*yp_get_buffer P((int));
static	int		bufno P((int));
static	Line		*copy_lines P((Line *, Line *));
static	char		*yanktext P((Posn *, Posn *));
static	void		yp_free P((Yankbuffer *));
static	bool_t		yp_chars_to_lines P((Yankbuffer *));
static	void		yp_lines_to_chars P((Yankbuffer *));
static	Line *		last_line_of P((Line *lp));
static	bool_t		append_str_to_lines P((Line **, char *));

void
init_yankput()
{
}

/*
 * From a yank buffer's single-character name, return a pointer to the
 * corresponding Yankbuffer structure.
 */
static Yankbuffer *
yp_get_buffer(name)
int	name;
{
    int	i;

    i = bufno(name);

    if (i < 0) {
	show_error("Invalid buffer name");
	return(NULL);
    }
    return(&yb[i]);
}

/*
 * bufno maps a yank buffer name into its index in yb[].
 *
 * Other than the call from yp_get_buffer(), it is only called with
 * constant buffer names, so the function should be optimised out at
 * compile time.
 */
static int
bufno(name)
int name;
{
    /* Map the two out-of-range chars into the space between 9 and @ */
    switch (name) {
    case '1': 	/* Valid names already in range */
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case ':':
    case '?':
    case '@':
    case '<':
    case '=': /* Secret buffer used by copy/move in exLineOperation() */
		break;

    case ';':	/* Invalid names that are in range */
    case '>': 	return -1;

		/* Valid names that are out of range, mapped to unused chars */
    case '!':	name = ';';	break;
    case '/':	name = '>';	break;

    default:
        /* Upper case buffer names are appending alises for the lower case ones */
	if (is_lower(name)) {
	    name = to_upper(name);
	} else if (!is_upper(name)) {
	    return(-1);
        }
    }

    if (name < LOWEST_NAME || name > HIGHEST_NAME) {
	return(-1);
    } else {
        return(name - LOWEST_NAME);
    }
}

/*
 * Yank the text specified by the given start/end positions.
 * The fourth parameter is TRUE if we are doing a character-
 * based, rather than a line-based, yank.
 *
 * For line-based yanks, the range of positions is inclusive.
 *
 * Returns TRUE if successfully yanked.
 *
 * Positions must be ordered properly, i.e. "from" <= "to".
 */

/* Helper functions for do_yank() */
static bool_t	copy_to_unnamed_yb P((Yankbuffer *));
static bool_t	yank_chars_to_yp P((Yankbuffer *, Posn *, Posn *));
static bool_t	append_chars_to_yp_buf P((Yankbuffer *, Yankbuffer *));

/*ARGSUSED*/
bool_t
do_yank(from, to, charbased, name)
Posn	*from, *to;
bool_t	charbased;
int	name;
{
    Yankbuffer	*yp_buf;
    bool_t	append;

    yp_buf = yp_get_buffer(name);
    if (yp_buf == NULL) {
	return(FALSE);
    }

    /*
     * If they specified an alphabetic buffer name in upper-case,
     * we should append to the yank buffer instead of replacing its contents.
     * According to POSIX, if you append a char-based yank to a line-based
     * buffer, the resulting yank buffer should be of the same type as the
     * newly-yanked text.
     * Appending to an empty buffer is the same as setting it.
     */
    append = is_upper(name) && (yp_buf->y_type != y_none);

    if (!append) {
	yp_free(yp_buf);
    }

    if (charbased) {
	Yankbuffer	new;

	/*
	 * To append, we yank into a private buffer and then construct the
	 * final buffer from the parts of the old and new yank buffers.
	 */
	if (!yank_chars_to_yp(append ? &new : yp_buf, from, to)) {
	    return(FALSE);
	}

	if (append) {
	    if (!append_chars_to_yp_buf(yp_buf, &new)) {
		return(FALSE);
	    }
	}
    } else {
	Line *newlines;

	/*
	 * Yank lines starting at "from", ending at "to".
	 */
	newlines = copy_lines(from->p_line, to->p_line->l_next);
	if (newlines == NULL) {
	    return(FALSE);
	}
	if (!append) {
	    yp_buf->y_line_buf = newlines;
	    yp_buf->y_type = y_lines;
	} else {
	    Line *last;

	    if (yp_buf->y_type == y_chars) {
		if (!yp_chars_to_lines(yp_buf)) {
		    return(FALSE);
		}
	    }
	    /*
	     * Find the last line in the yank buffer
	     * and add the new lines after it
	     */
	    last = last_line_of(yp_buf->y_line_buf);
	    last->l_next = newlines;
	}
    }

    /*
     * POSIX: "Commands that store text into buffers shall store the text
     * into the unnamed buffer as well as any specified buffer."
     * This only applies to the letter-named buffers, not the automatic ones.
     */
    if (is_alpha(name)) {
	if (!copy_to_unnamed_yb(yp_buf)) {
	    return(FALSE);
	}
    }

    return(TRUE);
}

/*
 * Copy a yank buffer to the unnamed/default buffer
 */
static bool_t
copy_to_unnamed_yb(yp_buf)
Yankbuffer	*yp_buf;
{
    Yankbuffer*	atp = &yb[bufno('@')];

    yp_free(atp);

    atp->y_type = yp_buf->y_type;

    if (yp_buf->y_1st_text != NULL) {
	if ((atp->y_1st_text = strsave(yp_buf->y_1st_text)) == NULL) {
	    goto oom1;
	}
	if (yp_buf->y_2nd_text != NULL) {
	    atp->y_2nd_text = strsave(yp_buf->y_2nd_text);
	    if (atp->y_2nd_text == NULL) {
		goto oom2;
	    }
	}
    }
    if (yp_buf->y_line_buf != NULL) {
	atp->y_line_buf = copy_lines(yp_buf->y_line_buf, (Line *) NULL);
	if (atp->y_line_buf == NULL) {
	    goto oom3;
	}
    }
    return(TRUE);

    /* Out-of-memory handlers */
oom3:
    free(atp->y_2nd_text);
    atp->y_2nd_text = NULL;
oom2:
    free(atp->y_1st_text);
    atp->y_1st_text = NULL;
oom1:
    return(FALSE);
}

/*
 * Yank the requested range in character mode into the given Yankbuffer.
 *
 * Returns: TRUE if successful, NULL on failure (due to memory exhaustion).
 */
static bool_t
yank_chars_to_yp(yp_buf, from, to)
Yankbuffer *yp_buf;
Posn	   *from, *to;
{
    long	nlines;
    Posn	ptmp;

    nlines = cntllines(from->p_line, to->p_line);

    /*
     * First yank either the whole of the text string
     * specified (if from and to are on the same line),
     * or from "from" to the end of the line.
     */
    ptmp.p_line = from->p_line;
    if (to->p_line == from->p_line) {
	ptmp.p_index = to->p_index;
    } else {
	ptmp.p_index = strlen(from->p_line->l_text) - 1;
    }
    yp_buf->y_1st_text = yanktext(from, &ptmp);
    if (yp_buf->y_1st_text == NULL) {
	return(FALSE);
    }

    /*
     * Next, determine if it is a multi-line character-based
     * yank, in which case we have to yank from the start of
     * the line containing "to" up to "to" itself.
     */
    if (nlines > 1) {
	ptmp.p_line = to->p_line;
	ptmp.p_index = 0;
	yp_buf->y_2nd_text = yanktext(&ptmp, to);
	if (yp_buf->y_2nd_text == NULL) {
	    free(yp_buf->y_1st_text);
	    return(FALSE);
	}
    } else {
	yp_buf->y_2nd_text = NULL;
    }

    /*
     * Finally, we may need to yank any lines between "from"
     * and "to".
     */
    if (nlines > 2) {
	yp_buf->y_line_buf =
	    copy_lines(from->p_line->l_next, to->p_line);
	if (yp_buf->y_line_buf == NULL) {
	    free(yp_buf->y_2nd_text);
	    free(yp_buf->y_1st_text);
	    return(FALSE);
	}
    } else {
	yp_buf->y_line_buf = NULL;
    }

    yp_buf->y_type = y_chars;

    return(TRUE);
}

/*
 * Append the new yank buffer to the old one.
 *
 * The resulting buffer is left in "old" in the same char/line mode
 * as the "new" text had.
 *
 * Alloc-ed lines of text are transferred from new to old by copying pointers
 * so no further deallocation is necessary of lines in "new".
 */
static bool_t
append_chars_to_yp_buf(old, new)
Yankbuffer *old;
Yankbuffer *new;
{
    if (old->y_type == y_lines) {
	yp_lines_to_chars(old);
    }

    /*
     * "old" contains the old yank buffer before the append and
     * "new" contains the newly-yanked lines to add to it.
     * In each one, 1st_text will always be set, 2nd_text may be set
     * and if it is, y_line_buf may also be set.
     * A first hack at the result was:
     * (old_1st, old_lines, old_2nd) + (new_1st, new_lines, new_2nd) =>
     * (old_1st, old_lines + old_2nd + new_1st + new_lines, new_2nd)
     * but it's not quite that simple. All the combinations are:
     *	   NEW	1st		1st 2nd		1st lines 2nd
     *	OLD
     *	1st	1st=old1st	1st=old1st	1st=old1st
     *		lines=NULL	lines=new1st	lines=new1st
     *						      newlines
     *		2nd=new1st	2nd=new2nd	2nd=new2nd
     *
     *	1st	1st=old1st	1st=old1st	1st=old1st
     *	2nd	lines=old2nd	lines=old2nd	lines=old2nd
     *		2nd=new1st	      new1st	      new1st
     *				2nd=new2nd	      newlines
     *						2nd=new2nd
     *
     *	1st	1st=old1st	1st=old1st	1st=old1st
     *	lines	lines=oldlines	lines=oldlines	lines=oldlines
     *	2nd	      old2nd	      old2nd	      old2nd
     *		2nd=new1st	      new1st	      new1st
     *				2nd=new2nd	      newlines
     *						2nd=new2nd
     * for which the simple formula above is right for columns 2 and 3,
     * but if new2nd is not set, the resulting 2nd comes from new1st.
     */

    /* 1st_text remains the same as before */

    /*
     * The old 2nd_text is appended to the list of lines
     */
    if (old->y_2nd_text != NULL) {
	if (!append_str_to_lines(&(old->y_line_buf),
				 old->y_2nd_text)) {
	    return(FALSE);
	}
    }

    /*
     * The new 1st_text is appended to the list of lines unless the new
     * 2nd_text is NULL, in which case it becomes 2nd_text in the
     * result (see below).
     */
    if (new->y_2nd_text != NULL) {
	if (!append_str_to_lines(&(old->y_line_buf),
				 old->y_1st_text)) {
	    return(FALSE);
	}
    }

    /*
     * Append the new lines to the list.
     * If new->y_line_buf is set, that means new->y_2nd_text was also
     * have been set, so old is sure to have a line buffer and
     * "last" is pointing to the last line in it.
     */
    if (new->y_line_buf != NULL) {
	Line *l = last_line_of(old->y_line_buf);
	l->l_next = new->y_line_buf;
	new->y_line_buf->l_prev = l;
    }

    /* The result's 2nd_text is that of the new yank, unless it's NULL
     * in which case the new 1st_text goes there (see above). */
    old->y_2nd_text = (new->y_2nd_text != NULL)
			 ? new->y_2nd_text : new->y_1st_text;

    return(TRUE);
}

/*
 * Yank the given string.
 *
 * Third parameter indicates whether to do it as a line or a string.
 *
 * Used for buffers '!', ':', '/', '?' and '<'.
 *
 * Returns TRUE if successfully yanked.
 */
bool_t
yank_str(name, str, line_based)
int	name;
char	*str;
bool_t	line_based;
{
    Yankbuffer		*yp_buf;
    register Line	*tmp;
    register char	*cp;

    yp_buf = yp_get_buffer(name);
    if (yp_buf == NULL) {
	return(FALSE);
    }
    yp_free(yp_buf);

    /*
     * Obtain space to store the string.
     */
    if (line_based) {
	/*
	 * First try to save the string. If no can do,
	 * return FALSE without affecting the current
	 * contents of the yank buffer.
	 */
	tmp = newline(strlen(str) + 1);
	if (tmp == NULL) {
	    return(FALSE);
	}
	tmp->l_prev = tmp->l_next = NULL;
	(void) strcpy(tmp->l_text, str);
    } else {
	cp = strsave(str);
	if (cp == NULL) {
	    return(FALSE);
	}
    }

    /*
     * Set up the yank structure.
     */
    if (line_based) {
	yp_buf->y_type = y_lines;
	yp_buf->y_line_buf = tmp;
    } else {
	yp_buf->y_type = y_chars;
	yp_buf->y_1st_text = cp;
    }

    return(TRUE);
}

/*
 * Put back the last yank at the specified position,
 * in the specified direction.
 */
void
do_put(location, direction, name, vi_cmd)
Posn	*location;
int	direction;
int	name;
Cmd *	vi_cmd;		/* If vi mode, the cmd; NULL if ex mode */
{
    Yankbuffer		*yp_buf;
    register Line	*currline;	/* line we are on now */
    register Line	*nextline;	/* line after currline */
    Buffer		*buffer;

    yp_buf = yp_get_buffer(name);
    if (yp_buf == NULL) {
	return;
    }

    buffer = curbuf;

    /*
     * Set up current and next line pointers.
     */
    currline = location->p_line;
    nextline = currline->l_next;

    /*
     * See which type of yank it was ...
     */
    switch (yp_buf->y_type) {

    case y_chars:
      {
	int	l;
	Posn	lastpos;
	Posn	cursorpos;

	if (!start_command(vi_cmd)) {
	    return;
	}

	l = curwin->w_cursor->p_index;
	if (direction == FORWARD && currline->l_text[l] != '\0') {
	    ++l;
	}

	/*
	 * Firstly, insert the 1st_text buffer, since this is
	 * always present. We may wish to split the line after
	 * the inserted text if this was a multi-line yank.
	 */
	replchars(currline, l, 0, yp_buf->y_1st_text);
	/* Cursor should end up on the first char of the newly-inserted text */
	cursorpos.p_line = currline;
	cursorpos.p_index = l;
	updateline(TRUE);
	lastpos.p_line = curwin->w_cursor->p_line;
	lastpos.p_index = curwin->w_cursor->p_index + strlen(yp_buf->y_1st_text);
	if (direction == BACKWARD) {
	    lastpos.p_index--;
	}

	/*
	 * If this is a multi-line character-based yank, we have to
	 * split the current line at the end of the first text segment,
	 * creating a new line to hold the remainder of what was on
	 * this line.
	 */
	if (yp_buf->y_2nd_text != NULL) {
	    int	end_of_1st_text;
	    Line	*newl;

	    end_of_1st_text = l + strlen(yp_buf->y_1st_text);
	    newl = newline(strlen(yp_buf->y_1st_text) + 1);
	    if (newl == NULL) {
		goto eoom;
	    }

	    /*
	     * Link the new line into the list.
	     */
	    repllines(nextline, 0L, newl);
	    nextline = newl;
	    replchars(nextline, 0, 0,
			currline->l_text + end_of_1st_text);
	    replchars(currline, end_of_1st_text,
			strlen(currline->l_text + end_of_1st_text), "");
	}

	if (yp_buf->y_line_buf != NULL) {
	    Line	*newlines;

	    lastpos.p_line = curwin->w_cursor->p_line->l_next;
	    newlines = copy_lines(yp_buf->y_line_buf, (Line *) NULL);
	    if (newlines == NULL) {
		goto eoom;
	    }
	    repllines(nextline, 0L, newlines);
	    lastpos.p_line = lastpos.p_line->l_prev;
	    lastpos.p_index = strlen(lastpos.p_line->l_text) - 1;
	}

	if (yp_buf->y_2nd_text != NULL) {
	    if (nextline == buffer->b_lastline) {
		Line	*new;

		/*
		 * Can't put the remainder of the text
		 * on the following line, 'cos there
		 * isn't one, so we have to create a
		 * new line.
		 */
		new = newline(strlen(yp_buf->y_2nd_text) + 1);
		if (new == NULL) {
		    goto eoom;
		}
		repllines(nextline, 0L, new);
		nextline = new;
	    }
	    replchars(nextline, 0, 0, yp_buf->y_2nd_text);
	    lastpos.p_line = nextline;
	    lastpos.p_index = strlen(yp_buf->y_2nd_text) - 1;
	}

	end_command();

	move_cursor(cursorpos.p_line, cursorpos.p_index);
	move_window_to_cursor();
	cursupdate();
	xvUpdateAllBufferWindows();
      }
      break;

    case y_lines:
      {
	Line	*new;		/* first line of lines to be put */

	/*
	 * Make a new copy of the saved lines.
	 */
	new = copy_lines(yp_buf->y_line_buf, (Line *) NULL);
	if (new == NULL) {
	    goto oom;
	}

	repllines((direction == FORWARD) ? nextline : currline, 0L, new);

	/*
	 * Put the cursor at the "right" place
	 * (i.e. the place the "real" vi uses).
	 */
	move_cursor(new, 0);
	begin_line(TRUE);
	move_window_to_cursor();
	cursupdate();
	xvUpdateAllBufferWindows();
      }
      break;
    default:
      show_error("Nothing to put!");
      break;
    }
    return;

eoom:
    /* Out of memory needing end_command() */
    end_command();
oom:
    /* "Out of memory" handler */
    return;
}

/*
 * Stuff the specified buffer into the input stream.
 * Called by the '@' command.
 *
 * The "vi_mode" parameter will be FALSE if the buffer should
 * be preceded by a ':' and followed by a '\n', i.e. it is the
 * result of a :@ command rather than a vi-mode @ command.
 *
 * The "map_it" parameter says whether we should put the characters
 * from the buffer through the map and/or map! tables.
 *
 * :@a and :*a give			!vi_mode && !map_it
 * @a gives				vi_mode && map_it
 * ^@ ^A or ^Ba during insert give	vi_mode && !map_it
 * redo of an insert or replace1 gives	vi_mode && !map_it
 *
 * POSIX for "@": "Behave as if the contents of the named buffer were entered
 * as standard input."
 *
 * POSIX: "After each line of a line-mode buffer,
 * and all but the last line of a character mode buffer,
 * behave as if a <newline> were entered as standard input."
 */
void
yp_stuff_input(name, vi_mode, map_it)
int	name;
bool_t	vi_mode;
bool_t	map_it;
{
    Yankbuffer	*yp_buf;
    Line	*lp;

    yp_buf = yp_get_buffer(name);
    if (yp_buf == NULL) {
	return;
    }

    if (!vi_mode) {	/* :@ ex command; map_it is always FALSE */
	/*
	 * POSIX for ":@": "For each line of a line-mode buffer, and all but
	 * the last line of a character-mode buffer, the ex command parser
	 * shall behave as if the line was terminated by a <newline>."
	 *
	 * Classic vi and nvi don't add a newline to the last line of a
	 * multi-line char-mode buffer, but do add one to a char-mode buffer
	 * containing a single line
	 *
	 * vim adds a newline to the last line of any character-mode buffer
	 * stuffed with :@ or :*.
	 *
	 * hanoi and maze work either way, but it seems pointless to stuff
	 * an ex command and leave the last line hanging there waiting for
	 * the user to preee [Enter] so by default we newline-terminate
	 * all lines of a :@, and do what POSIX wants if $POSIXLY_CORRECT.
	 */
	bool_t posixly_correct = Pb(P_posix);

	switch (yp_buf->y_type) {
	case y_chars:
	    put(yp_buf->y_1st_text, vi_mode,
		posixly_correct ? yp_buf->y_2nd_text != NULL : TRUE,
		map_it);
	    break;

	case y_lines:
	    break;

	default:
	    show_error("Nothing to put!");
	    return;
	}

	for (lp = yp_buf->y_line_buf; lp != NULL; lp = lp->l_next) {
	    put(lp->l_text, vi_mode, TRUE, map_it);
	}

	if (yp_buf->y_type == y_chars && yp_buf->y_2nd_text != NULL) {
	    put(yp_buf->y_2nd_text, vi_mode,
		posixly_correct ? FALSE : TRUE,
		map_it);
	}
    } else {
	/*
	 * Characters from @f commands are stuffed into the start of
	 * the canonical queue so here we have to put() the elements in
	 * reverse order for them to end up in the correct order.
	 */
	switch (yp_buf->y_type) {
	case y_chars:
	    if (yp_buf->y_2nd_text != NULL) {
		put(yp_buf->y_2nd_text, vi_mode, FALSE, map_it);
	    }
	    break;

	case y_lines:
	    break;

	default:
	    show_error("Nothing to put!");
	    return;
	}

	/* Insert the intervening lines in reverse order */
	if (yp_buf->y_line_buf != NULL) {
	    /* Go to the last element in the list of lines */
	    for (lp = yp_buf->y_line_buf; lp->l_next != NULL; lp = lp->l_next)
		;
	    /*
	     * Now work back towards the first one, including the first line.
	     * The (lp = lp->l_prev) != NULL is always true, used to make it
	     * go back to the first line inclusive.
	     */
	    do {
		put(lp->l_text, vi_mode, TRUE, map_it);
	    } while (lp != yp_buf->y_line_buf && (lp = lp->l_prev));
	}

	if (yp_buf->y_type == y_chars && yp_buf->y_1st_text != NULL) {
	    put(yp_buf->y_1st_text, vi_mode, yp_buf->y_2nd_text != NULL, map_it);
	}
    }
}

/*
 * Stuff a string into the input.
 *
 * If vi_mode is FALSE, we're stuffing ex commands (the :@f command) so need
 * to add a leading : if it's missing.
 *
 * If map_it is TRUE, we are stuffing input that needs to go through the
 * :map and :map! tables. If FALSE, it's already been there (the Redo insert).
 */
static void
put(str, vi_mode, newline, map_it)
char	*str;
bool_t	vi_mode;
bool_t	newline;
bool_t	map_it;
{
    if (vi_mode) {
	/*
	 * stuff_to_map() inserts chars at the start of canon_queue so
	 * insert a trailing newline, if any, before the text so that it
	 * ends up after the text that it should follow.
	 */
	if (map_it) {
	    if (newline) {
		stuff_to_map("\n");
	    }
	    stuff_to_map(str);
	} else {
	    stuff(newline ? "%s\n" : "%s", str);
	}
    } else {
	/* !vi_mode is exclusive to the :@ / :* commands. */
	stuff(":%s%s", str, newline ? "\n" : "");
    }
}

/*
 * Copy the lines pointed at by "from", up to but not including
 * pointer "to" (which might be NULL), into new memory and return
 * a pointer to the start of the new list.
 *
 * Returns NULL for errors.
 */
static Line *
copy_lines(from, to)
Line	*from, *to;
{
    Line	*src;
    Line	head;
    Line	*dest = &head;

    head.l_next = NULL;

    for (src = from; src != to; src = src->l_next) {
	Line	*tmp;

	tmp = newline(strlen(src->l_text) + 1);
	if (tmp == NULL) {
	    throw(head.l_next);
	    return(NULL);
	}

	/*
	 * Copy the line's text over, and advance
	 * "dest" to point to the new line structure.
	 */
	(void) strcpy(tmp->l_text, src->l_text);
	tmp->l_next = NULL;
	tmp->l_prev = dest;
	dest->l_next = tmp;
	dest = tmp;
    }

    return(head.l_next);
}

static char *
yanktext(from, to)
Posn	*from, *to;
{
    int		nchars;
    char	*cp;

    nchars = to->p_index - from->p_index + 1;
    cp = alloc((unsigned) nchars + 1);
    if (cp == NULL) {
	return(NULL);
    }

    (void) memcpy(cp, from->p_line->l_text + from->p_index, nchars + 1);
    cp[nchars] = '\0';

    return(cp);
}

static void
yp_free(yp)
Yankbuffer	*yp;
{
    switch (yp->y_type) {
    case y_chars:
	free(yp->y_1st_text);
	yp->y_1st_text = NULL;
	free(yp->y_2nd_text);
	yp->y_2nd_text = NULL;
    case y_lines:
	throw(yp->y_line_buf);
	yp->y_line_buf = NULL;
    case y_none:
	;
    }
    yp->y_type = y_none;
}

/*
 * Push up buffers 1..8 by one, spilling 9 off the top.
 * Then move '@' into '1'.
 */
void
yp_push_deleted()
{
    Yankbuffer	*atp;
    Yankbuffer	*ybp;

    yp_free(&yb[bufno('9')]);
    for (ybp = &yb[bufno('8')]; ybp >= &yb[bufno('1')]; ybp--) {
	ybp[1] = ybp[0];
    }
    atp = &yb[bufno('@')];
    yb[bufno('1')] = *atp;
    atp->y_type = y_none;
    atp->y_line_buf = NULL;
    atp->y_1st_text = NULL;
    atp->y_2nd_text = NULL;
}

/*
 * Convert a character-based buffer to a line-based one.
 *
 * A char buffer may be just 1st_text, just 1st_ and 2nd_text or all three;
 * in all three cases, the 1st (and 2nd if present) text are converted to lines.
 */
static bool_t
yp_chars_to_lines(yp)
Yankbuffer	*yp;
{
    Line *y_1st_line;	/* 1st_text as a line */
    Line *y_2nd_line;	/* 2nd_text as a Line,
			 * only used if (yp->y_2nd_text != NULL) */

    /* Allocate all new space first so that we can do all or nothing */

    /* Convert the first line */
    y_1st_line = snewline(yp->y_1st_text);
    if (y_1st_line == NULL) {
	return(FALSE);
    }
    /*
     * Allocate the second line's storage early so that if it fails
     * we can fail without having changed anything.
     */
    if (yp->y_2nd_text != NULL) {
	y_2nd_line = snewline(yp->y_2nd_text);
	if (y_2nd_line == NULL) {
	    /* It takes more code to free 1st_line than the space it occupies */
	    return(FALSE);
	}
    }

    /* Add the 1st line to the start of the line_buf */
    if (yp->y_line_buf == NULL) {
	yp->y_line_buf = y_1st_line;
    } else {
	y_1st_line->l_next = yp->y_line_buf;
	yp->y_line_buf->l_prev = y_1st_line;
	yp->y_line_buf = y_1st_line;
    }

    /* Convert a second line, if any, and add it to the end of line_buf */
    if (yp->y_2nd_text != NULL) {
        Line *last;

	last = last_line_of(yp->y_line_buf);
	y_2nd_line->l_prev = last;
	last->l_next = y_2nd_line;
    }

    yp->y_1st_text = yp->y_2nd_text = NULL;
    yp->y_type = y_lines;
    return(TRUE);
}

/*
 * Convert a line-based buffer to a character-based one.
 *
 * A char buffer may be just 1st_text, just 1st_ and 2nd_text or all three,
 * so we need to split off the first and last lines of the line buffer into those.
 */
static void
yp_lines_to_chars(yp)
Yankbuffer	*yp;
{
    Line *lp = yp->y_line_buf;	/* First line */

    /* Split of first line's text into y_1st_text */
    yp->y_1st_text = lp->l_text;
    yp->y_line_buf = lp->l_next;
    /* Free just the Line object */
    lp->l_text = NULL;
    lp->l_next = NULL;
    throw(lp);

    /* If there were two or more lines, split off the last one into 2nd_text */
    if (yp->y_line_buf != NULL) {
	Line **lpp;	/* Points to the l_next cell of the penultimate line */

	for (lpp=&(yp->y_line_buf); (*lpp)->l_next != NULL; lpp=&((*lpp)->l_next)) ;

	/* Move the last line's text to 2nd_text */
	yp->y_2nd_text = (*lpp)->l_text;
	(*lpp)->l_text = NULL;

	/* and drop the last Line from the list */
	throw(*lpp);
	*lpp = NULL;
    }

    yp->y_type = y_chars;
}

/*
 * Return the last line of a Line buffer.
 * The argument must be a valid pointer to a Line.
 */
static Line *
last_line_of(lines)
Line *lines;
{
    register Line *lp;

    for (lp = lines; lp->l_next != NULL; lp = lp->l_next)
	;
    return(lp);
}

/*
 * Take a string, make a Line out of it and append it to a line buffer.
 * If the line buffer is NULL, it will be modified to point to the single
 * new line.
 */
static bool_t
append_str_to_lines(linesp, str)
Line **linesp;
char *str;
{
    Line *last;	/* last line in buffer */
    Line *l;

    l = snewline(str);
    if (!l) return(FALSE);

    if (*linesp == NULL) {
	*linesp = l;
    } else {
	/* Put it after the last existing line */
	last = last_line_of(*linesp);
	last->l_next = l;
	l->l_prev = last;
    }

    return(TRUE);
}
