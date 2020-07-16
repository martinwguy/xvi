/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    find.c
* module function:
    Character and function searching.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"
#include "regexp.h"	/* Henry Spencer's regular expression routines */

#ifdef	MEGAMAX
overlay "find"
#endif

/*
 * Character Searches
 */

static char lastc;		/* last character searched for */
static int  lastcdir;		/* last direction of character search */
static int  lastctype;		/* last type of search ("find" or "to") */

/*
 * searchc(c, dir, type, num)
 *
 * Search for character 'c', in direction 'dir'. If type is 0, move to
 * the position of the character, otherwise move to just before the char.
 * 'num' is the number of times to do it (the prefix number).
 */
Posn *
searchc(ch, dir, type, num)
int	ch;
int	dir;
int	type;
int	num;
{
    static Posn		pos;			/* saved cursor posn */
    enum mvtype		(*mvfunc) P((Posn *));
    enum mvtype		(*backfunc) P((Posn *));
    unsigned char	c;			/* as ch but uchar */
    register int	i;			/* loop counter */

    /*
     * Remember details in case we want to repeat the search.
     */
    lastc = ch;
    lastcdir = dir;
    lastctype = type;

    pos = *(curwin->w_cursor);	/* save position in case we fail */
    c = ch;
    if (dir == FORWARD) {
	mvfunc = inc;
	backfunc = dec;
    } else {
	mvfunc = dec;
	backfunc = inc;
    }

    for (i = 0; i < num; i++) {
	bool_t	found;

	found = FALSE;
	while ((*mvfunc)(&pos) == mv_SAMELINE) {
	    if ((unsigned char) gchar(&pos) == c) {
		found = TRUE;
		break;
	    }
	}
	if (!found) {
	    return(NULL);
	}
    }
    if (type) {
	(void) (*backfunc)(&pos);
    }
    return(&pos);
}

/*ARGSUSED*/
Posn *
crepsearch(buffer, flag, num)
Buffer	*buffer;
int	flag;
int	num;
{
    Posn	*newpos;
    int	dir;
    int	savedir;

    if (lastc == '\0') {
	return(NULL);
    }

    savedir = lastcdir;
    if (flag) {
	dir = (lastcdir == FORWARD) ? BACKWARD : FORWARD;
    } else {
	dir = lastcdir;
    }

    newpos = searchc((int) lastc, dir, lastctype, num);

    lastcdir = savedir;		/* put direction of search back how it was */

    return(newpos);
}

/*
 * "Other" Searches
 */

/*
 * showmatch - move the cursor to the matching paren or brace
 */
Posn *
showmatch()
{
    register char	initc;			/* initial char */
    register enum mvtype
			(*move) P((Posn *));	/* function to move cursor */
    register char	findc;			/* terminating char */
    static Posn		pos;			/* current position */
    char		c;
    int			count = 0;
    bool_t		found = FALSE;

    pos = *curwin->w_cursor;

    /*
     * Move forward to the first bracket character after the cursor.
     * If we get to EOF before a bracket, return NULL.
     */
    for (found = FALSE; !found; ) {
	initc = gchar(&pos);
	switch (initc) {
	case '(':
	    findc = ')';
	    move = inc;
	    found = TRUE;
	    break;
	case ')':
	    findc = '(';
	    move = dec;
	    found = TRUE;
	    break;
	case '{':
	    findc = '}';
	    move = inc;
	    found = TRUE;
	    break;
	case '}':
	    findc = '{';
	    move = dec;
	    found = TRUE;
	    break;
	case '[':
	    findc = ']';
	    move = inc;
	    found = TRUE;
	    break;
	case ']':
	    findc = '[';
	    move = dec;
	    found = TRUE;
	    break;
	default:
	    if (inc(&pos) == mv_NOMOVE) {
		return(NULL);
	    }
	}
    }

    /*
     * Move in the appropriate direction until we find a matching
     * bracket or reach end of file.
     */
    while ((*move)(&pos) != mv_NOMOVE) {
	c = gchar(&pos);
	if (c == initc) {
	    count++;
	} else if (c == findc) {
	    if (count == 0)
		return(&pos);
	    count--;
	}
    }
    return(NULL);			/* never found it */
}

/*
 * The following routines do the word searches performed by the
 * 'w', 'W', 'b', 'B', 'e', and 'E' commands.
 */

/*
 * To perform these searches, characters are placed into one of three
 * classes, and transitions between classes determine word boundaries.
 *
 * The classes are:
 *
 * cl_white - white space
 * cl_text  - letters, digits, and underscore
 * cl_punc  - everything else
 */

typedef enum {
    cl_white,
    cl_text,
    cl_punc
} cclass;

static	int	stype;		/* type of the word motion being performed */

#define is_white(c)	(((c) == ' ') || ((c) == '\t') || ((c) == '\0'))
#define is_text(c)	(is_alnum(c) || ((c) == '_'))

/*
 * cls(c) - returns the class of character 'c'
 *
 * The 'type' of the current search modifies the classes of characters
 * if a 'W', 'B', or 'E' motion is being done. In this case, chars. from
 * class "cl_punc" are reported as class "cl_text" since only white space
 * boundaries are of interest.
 */
static cclass
cls(c)
char	c;
{
    if (is_white(c))
	return(cl_white);

    if (is_text(c))
	return(cl_text);

    /*
     * If stype is non-zero, report these as class 1.
     */
    return((stype == 0) ? cl_punc : cl_text);
}


/*
 * fwd_word(pos, type, skip_white) - move forward one word
 *
 * Returns the resulting position, or NULL if EOF was reached.
 *
 * The extra argument "skip_white" (which is only used in fwd_word,
 * but also included in bck_word and end_word for compatibility) is
 * used to indicate whether to skip over white space to get to the
 * start of the next word. If it is FALSE, the position returned will
 * be the first white-space character (or punctuation) encountered.
 * This is used by one command only: "cw".
 */
Posn *
fwd_word(p, type, skip_white)
Posn	*p;
int	type;
bool_t	skip_white;
{
    static	Posn	pos;
    cclass		sclass;		/* starting class */

    stype = type;
    sclass = cls(gchar(p));

    pos = *p;

    /*
     * We always move at least one character.
     */
    if (inc(&pos) == mv_NOMOVE)
	return(NULL);

    if (sclass != cl_white) {
	/*
	 * We were in the middle of a word to start with.
	 * Move right until we change character class.
	 */
	while (cls(gchar(&pos)) == sclass) {
	    if (inc(&pos) == mv_NOMOVE) {
		/*
		 * Got to EOF. Return current position.
		 */
		return(&pos);
	    }
	}

	/*
	 * If we went from punctuation -> text
	 * or text -> punctuation, return here.
	 *
	 * If we went text/punctuation -> whitespace,
	 * we want to continue to the start of the
	 * next word, if there is one.
	 */
	if (cls(gchar(&pos)) != cl_white)
	    return(&pos);
    }

    /*
     * We're in white space; go to next non-white.
     */
    if (skip_white) {
	while (cls(gchar(&pos)) == cl_white) {
	    /*
	     * We'll stop if we land on a blank line
	     */
	    if (pos.p_index == 0 && pos.p_line->l_text[0] == '\0')
		break;

	    if (inc(&pos) == mv_NOMOVE) {
		/*
		 * We have reached end of file; if we are at
		 * the beginning of a line, just return that
		 * position, otherwise try to back up so that
		 * we are still within the line.
		 */
		if (pos.p_index != 0) {
		    (void) dec(&pos);
		}
		break;
	    }
	}

	/*
	 * If we didn't move, return NULL.
	 */
	if (pos.p_line == p->p_line && pos.p_index == p->p_index) {
	    return(NULL);
	}
    }

    return(&pos);
}

/*
 * bck_word(pos, type, skip_white) - move backward one word
 *
 * Returns the resulting position, or NULL if EOF was reached.
 */
/*ARGSUSED*/
Posn *
bck_word(p, type, skip_white)
Posn	*p;		/* whence */
int	type;		/* 0 for lower case commands, 1 for upper */
bool_t	skip_white;	/* TRUE for everything except "cw" */
{
    static	Posn	pos;
    cclass		sclass;		/* starting class */

    stype = type;
    sclass = cls(gchar(p));

    pos = *p;

    if (dec(&pos) == mv_NOMOVE)
	return(NULL);

    /*
     * If we're in the middle of a word, we just have to
     * back up to the start of it.
     */
    if (cls(gchar(&pos)) == sclass && sclass != cl_white) {
	/*
	 * Move backward to start of the current word
	 */
	while (cls(gchar(&pos)) == sclass) {
	    if (dec(&pos) == mv_NOMOVE)
		return(&pos);
	}
	(void) inc(&pos);	/* overshot - forward one */
	return(&pos);
    }

    /*
     * We were at the start of a word. Go back to the start
     * of the prior word.
     */

    while (cls(gchar(&pos)) == cl_white) {	/* skip any white space */
	/*
	 * We'll stop if we land on a blank line
	 */
	if (pos.p_index == 0 && pos.p_line->l_text[0] == '\0')
	    return(&pos);

	if (dec(&pos) == mv_NOMOVE)
	    return(&pos);
    }

    sclass = cls(gchar(&pos));

    /*
     * Move backward to start of this word.
     */
    while (cls(gchar(&pos)) == sclass) {
	if (dec(&pos) == mv_NOMOVE)
	    return(&pos);
    }
    (void) inc(&pos);		/* overshot - forward one */

    return(&pos);
}

/*
 * end_word(pos, type, skip_white) - move to the next end-of-word after
 *				     the current cursor position
 *
 * There is an apparent bug in the 'e' motion of the real vi. At least
 * on the System V Release 3 version for the 80386. Unlike 'b' and 'w',
 * the 'e' motion crosses blank lines. When the real vi crosses a blank
 * line in an 'e' motion, the cursor is placed on the FIRST character
 * of the next non-blank line. The 'E' command, however, works correctly.
 * Since this appears to be a bug, I have not duplicated it here.
 *
 * Returns the resulting position, or NULL if EOF was reached.
 */
/*ARGSUSED*/
Posn *
end_word(p, type, skip_white)
Posn	*p;
int	type;
bool_t	skip_white;
{
    static	Posn	pos;
    cclass		sclass;

    stype = type;
    sclass = cls(gchar(p));

    pos = *p;

    switch (inc(&pos)) {
    case mv_NOMOVE:
	return(NULL);
    case mv_EOL:
	/* At end of file we get EOL then NOMOVE. */
	if (inc(&pos) == mv_NOMOVE) return(NULL);
	else dec(&pos);
    default:
	;
    }

    /*
     * If we're in the middle of a word, we just have to
     * move to the end of it.
     */
    if (cls(gchar(&pos)) == sclass && sclass != cl_white) {
	/*
	 * Move forward to end of the current word
	 */
	while (cls(gchar(&pos)) == sclass) {
	    if (inc(&pos) == mv_NOMOVE) {
		return(&pos);
	    }
	}
	(void) dec(&pos);		/* overshot - forward one */
	return(&pos);
    }

    /*
     * We were at the end of a word. Go to the end
     * of the next word.
     */

    while (cls(gchar(&pos)) == cl_white) {	/* skip any white space */
	if (inc(&pos) == mv_NOMOVE) {
	    return(&pos);
	}
    }

    sclass = cls(gchar(&pos));

    /*
     * Move forward to end of this word.
     */
    while (cls(gchar(&pos)) == sclass) {
	if (inc(&pos) == mv_NOMOVE) {
	    return(&pos);
	}
    }
    (void) dec(&pos);			/* overshot - forward one */

    return(&pos);
}

Posn *
xvDoSearch(str, cmd_char)
char		*str;
int		cmd_char;
{
    Xviwin	*window = curwin;
    Posn	*p;
    unsigned	savecho;
    Posn	*retpos;
    int		dir;
    static int	lastdir = FORWARD;

    /*
     * Place the cursor at bottom left of the window,
     * so the user knows what we are doing.
     */
    gotocmd(FALSE);

    switch (cmd_char) {
    case '/':
	lastdir = dir = FORWARD;
	break;
    case '?':
	lastdir = dir = BACKWARD;
	break;
    case 'n':
	dir = lastdir;
	break;
    case 'N':
	dir = (lastdir == FORWARD) ? BACKWARD : FORWARD;
	break;
    }

    /*
     * It is safe not to put the cursor back, because
     * we are going to produce some more output anyway.
     */

    savecho = echo;

    p = search(window->w_cursor->p_line,
	       window->w_cursor->p_index, dir, &str);
    if (p == NULL) {
	regerror("Pattern not found");
	retpos = NULL;
    } else if (*str != '\0') {
	regerror("Usage: /pattern or ?pattern");
	retpos = NULL;
    } else {
	retpos = p;
    }

    echo = savecho;
    return(retpos);
}

static void
xvMoveToNextNonWhite(pos)
Posn	*pos;
{
    char	c;

    /*
     * Move over any punctuation etc. to get to whitespace.
     */
    do {
	c = gchar(pos);
	if (c == '\0' || is_space(c)) {
	    break;
	}
    } while (inc(pos) != mv_NOMOVE);

    /*
     * Move over whitespace until we reach a real non-white character.
     */
    do {
	c = gchar(pos);
	if (c != '\0' && !is_space(c)) {
	    break;
	}
    } while (inc(pos) != mv_NOMOVE);
}

Posn *
xvLocateTextObject(startpos, opchar1, opchar2)
Posn	*startpos;
int	opchar1, opchar2;
{
    int		dir = FORWARD;
    char	*pattern;
    Posn	*newpos;
    static Posn	store_pos;

    switch (opchar1) {
    case '(':
	dir = BACKWARD;
	/*FALLTHROUGH*/
    case ')':
	pattern = Ps(P_sentences);
	break;

    case '{':
	dir = BACKWARD;
	/*FALLTHROUGH*/
    case '}':
	pattern = Ps(P_paragraphs);
	break;

    case '[':
	dir = BACKWARD;
	/*FALLTHROUGH*/
    case ']':
	if (opchar1 != opchar2) {
	    return(NULL);
	} else {
	    pattern = Ps(P_sections);
	}
    }

    /*
     * For the ) command, we initially find the pattern backwards.
     * This is so that if we happen to be between the end of one
     * sentence and the start of the next, we will move the cursor
     * to the next start of sentence, even though the pattern is
     * actually found backwards.
     */
    newpos = NULL;
    if (opchar1 == ')') {
	newpos = xvFindPattern(startpos, pattern, BACKWARD, TRUE);
    }

    /*
     * Default case, or if no match is found backward for ')'.
     */
    if (newpos == NULL) {
	newpos = xvFindPattern(startpos, pattern, dir, FALSE);
    }

    /*
     * Sentences are defined by an ending pattern, but we
     * actually move to the start of the next sentence,
     * ignoring any intervening whitespace.
     */
    if (newpos != NULL && (opchar1 == '(' || opchar1 == ')')) {
	store_pos = *newpos;
	xvMoveToNextNonWhite(newpos);
	while ((dir == BACKWARD) ?
		    !lt(newpos, startpos) : !lt(startpos, newpos)) {
	    /*
	     * Make sure we don't sit on our current position.
	     */
	    newpos = xvFindPattern(&store_pos, pattern, dir, FALSE);
	    if (newpos != NULL) {
		store_pos = *newpos;
		xvMoveToNextNonWhite(newpos);
	    } else {
		break;
	    }
	}
    }

    /*
     * Now ensure store_pos is set to a valid position.
     * This is what normally gets returned, unless it is overridden.
     */
    if (newpos != NULL) {
	store_pos = *newpos;
    } else {
	/*
	 * Pattern not found; default to start or end of file.
	 */
	switch (dir) {
	case BACKWARD:
	    store_pos.p_line = curbuf->b_file;
	    store_pos.p_index = 0;	/* should begin_line() if [[ or ]] ? */
	    break;

	case FORWARD:
	    {
		enum mvtype mv;

		store_pos.p_line = b_last_line_of(curbuf);
		store_pos.p_index = 0;
		while ((mv = inc(&store_pos)) != mv_EOL && mv != mv_NOMOVE)
		    ;
		/*
		 * Don't go beyond the end of the line.
		 */
		if (mv == mv_EOL && store_pos.p_index > 0) {
		    (void) dec(&store_pos);
		}
	    }
	}
    }

    /*
     * Tricky stuff this. Section boundaries are also paragraph
     * boundaries, and both section and paragraph boundaries are
     * also sentence boundaries. So we have to look to see what
     * boundary gets hit first, and choose that one.
     */

    if (opchar1 == '(' || opchar1 == ')') {
	/*
	 * Paragraph boundaries are also sentence boundaries.
	 */
	newpos = xvFindPattern(startpos, Ps(P_paragraphs), dir, FALSE);
	if (
	    newpos != NULL
	    &&
	    (
		(dir == BACKWARD) ?
		    lt(&store_pos, newpos) : lt(newpos, &store_pos)
	    )
	) {
	    store_pos = *newpos;
	}
    }

    if (opchar1 != '[' && opchar1 != ']') {
	/*
	 * Section boundaries are also paragraph and sentence boundaries.
	 */
	newpos = xvFindPattern(startpos, Ps(P_sections), dir, FALSE);
	if (
	    newpos != NULL
	    &&
	    (
		(dir == BACKWARD) ?
		    lt(&store_pos, newpos) : lt(newpos, &store_pos)
	    )
	) {
	    store_pos = *newpos;
	}
    }

    return(&store_pos);
}
