/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    search.c
* module function:
    Regular expression searching, including global command.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"
#include "regexp.h"	/* Henry Spencer's regular expression routines */
#include "regmagic.h"	/* Henry Spencer's regular expression routines */

#ifdef	MEGAMAX
overlay "search"
#endif

/*
 * String searches
 *
 * The actual searches are done using Henry Spencer's regular expression
 * library.
 */

/*
 * Names of values for the P_regextype enumerated parameter.
 */
char	*rt_strings[] =
{
    "tags",
    "grep",
    "egrep",
    NULL
};

/*
 * Used by g/re/p to remember where we are and what we are doing.
 */
static	Line	*curline;
static	Line	*lastline;
static	long	curnum;
static	bool_t	greptype;

/*
 * Last rhs for a substitution.
 */
static	char	*last_rhs = NULL;

static	Posn	*match P((Line *, int));
static	Posn	*bcksearch P((Line *, int, bool_t));
static	Posn	*fwdsearch P((Line *, int, bool_t));
static	char	*mapstring P((char **, int));
static	char	*compile P((char *, int, bool_t));
static	char	*grep_line P((void));
static	long	substitute P((Line *, Line *, char *, char *));
static	void	add_char_to_rhs P((Flexbuf *dest, int c, int ulmode));

/*
 * Convert a regular expression to egrep syntax: the source string can
 * be either tags compatible (only ^ and $ are significant), vi
 * compatible or egrep compatible (but also using \< and \>)
 *
 * Our first parameter here is the address of a pointer, which we
 * point to the closing delimiter character if we found one, otherwise
 * the closing '\0'.
 */
static char *
mapstring(sp, delim)
char	**sp;		/* pointer to pointer to pattern string */
int	delim;		/* delimiter character */
{
    static Flexbuf	ns;
    int			rxtype;	/* can be rt_TAGS, rt_GREP or rt_EGREP */
    register enum {
	m_normal,	/* nothing special */
	m_startccl,	/* just after [ */
	m_negccl,	/* just after [^ */
	m_ccl,		/* between [... or [^... and ] */
	m_escape	/* just after \ */
    }	state = m_normal;
    register int	c;
    register char	*s;

    rxtype = Pn(P_regextype);

    flexclear(&ns);
    for (s = *sp; (c = *s) != '\0' && (c != delim || state != m_normal); s++) {
	switch (state) {
	case m_normal:
	    switch (c) {
	    case '\\':
		state = m_escape;
		break;

	    case '~':
		/* ~, the last replacement text, is funny as it doesn't have
		 * an egrep equivelent. We expand it here.
		 * It is special unadorned if magic, special if \~ when nomagic.
		 */
		if (rxtype != rt_TAGS) {
		    /* Replace the ~ with the last replacement text */
		    lformat(&ns, "%s", last_rhs); /* If NULL, inserts nothing */
		} else {
		    (void) flexaddch(&ns, c);
		}
		break;

	    case '^':
	    {
		/* Previous char in the output */
#define		prevc ((ns.fxb_wcnt <= ns.fxb_rcnt) ? '\0' : \
			ns.fxb_chars[ns.fxb_wcnt-1])

		/* The char before that in the output */
#define		pprevc ((ns.fxb_wcnt <= ns.fxb_rcnt+1) ? '\0' : \
			      ns.fxb_chars[ns.fxb_wcnt-2])

		/*
		 * A '^' should only have special meaning if it comes
		 * at the start of the pattern (or subpattern, if we're
		 * doing egrep-style matching).
		 */
		if (prevc != '\0' &&
		    (rxtype != rt_EGREP ||
			(prevc != '|' && prevc != '(' /*)*/ )
			 || pprevc == '\\')) {
		    (void) flexaddch(&ns, '\\');
		}
		(void) flexaddch(&ns, '^');
#undef prevc
#undef pprevc

		break;
	    }

	    case '$':
	    {
		int	nextc = s[1];
		/*
		 * A '$' should only have special meaning if it comes
		 * at the end of the pattern (or subpattern, if we're
		 * doing egrep-style matching).
		 */
		if (nextc != '\0' && nextc != delim &&
			(rxtype != rt_EGREP ||
			    (nextc != '|' && /*(*/ nextc != ')'))) {
		    (void) flexaddch(&ns, '\\');
		}
		(void) flexaddch(&ns, '$');
		break;
	    }

	    case '(': case ')': case '+': case '?': case '|':
		/* egrep metacharacters */
		if (rxtype != rt_EGREP)
		    (void) flexaddch(&ns, '\\');
		(void) flexaddch(&ns, c);
		break;

	    case '*': case '.': case '[':
		/* grep metacharacters */
		if (rxtype == rt_TAGS) {
		    (void) flexaddch(&ns, '\\');
		} else if (c == '[') {
		    /* start of character class */
		    state = m_startccl;
		}
		 /* fall through ... */

	    default:
		(void) flexaddch(&ns, c);
	    }
	    break;

	case m_startccl:
	case m_negccl:
	    (void) flexaddch(&ns, c);
	    state = (c == '^' && state == m_startccl) ? m_negccl : m_ccl;
	    break;

	case m_ccl:
	    (void) flexaddch(&ns, c);
	    if (c == ']')
		state = m_normal;
	    break;

	case m_escape:
	    switch (c) {
	    case '(':		/* bracket conversion */
	    case ')':
		if (rxtype != rt_GREP)
		    (void) flexaddch(&ns, '\\');
		(void) flexaddch(&ns, c);
		break;

	    case '~':
		if (rxtype == rt_TAGS) {
		    /* Replace the ~ with the last replacement text */
		    lformat(&ns, "%s", last_rhs); /* If NULL, inserts nothing */
		    break;
		} else {
		    /* Drop through... */
		}

	    case '.':		/* egrep metacharacters */
	    case '\\':
	    case '[':
	    case '*':
	    case '?':
	    case '+':
	    case '^':
	    case '$':
	    case '|':
		(void) lformat(&ns, "\\%c", c);
		break;

	    default:		/* a normal character */
		if (c != delim)
		    (void) flexaddch(&ns, '\\');
		(void) flexaddch(&ns, c);
	    }
	    state = m_normal;
	}
    }

    *sp = s;

    if (state == m_escape) {
	/*
	 * This is horrible, but the real vi does it, so ...
	 */
	(void) lformat(&ns, "\\\\");
    }

    return flexgetstr(&ns);
}

/**********************************************************
 *							  *
 * Abstract type definition.				  *
 *							  *
 * Regular expression node, with pointer reference count. *
 *							  *
 * We need this for global substitute commands.		  *
 *							  *
 **********************************************************/

typedef struct {
    regexp	*rn_ptr;
    int		rn_count;
} Rnode;

/*
 * Node for last successfully compiled regular expression.
 */
static Rnode	*lastprogp = NULL;

/*
 * Last regular expression used in a substitution.
 */
static	Rnode	*last_lhs = NULL;

/*
 * rn_new(), rn_delete() & rn_duplicate() perform operations on Rnodes
 * which are respectively analogous to open(), close() & dup() for
 * Unix file descriptors.
 */

/*
 * Make a new Rnode, given a pattern string.
 */
static Rnode *
rn_new(str)
    char	*str;
{
    Rnode	*retp;

    if ((retp = alloc(sizeof (Rnode))) == NULL)
	return NULL;
    if ((retp->rn_ptr = regcomp(str)) == NULL) {
	free (retp);
	return NULL;
    }
    retp->rn_count = 1;
    return retp;
}

/*
 * Make a copy of an Rnode pointer & increment the Rnode's reference
 * count.
 */
#define rn_duplicate(s)	((s) ? ((s)->rn_count++, (s)) : NULL)

/*
 * Decrement an Rnode's reference count, freeing it if there are no
 * more pointers pointing to it.
 *
 * In C++, this would be a destructor for an Rnode.
 */
static void
rn_delete(rp)
Rnode	*rp;
{
    if (rp != NULL && --rp->rn_count <= 0) {
	free(rp->rn_ptr);
	free(rp);
    }
}

#define	cur_prog()	(lastprogp->rn_ptr)

/*
 * Compile given regular expression from string.
 *
 * The opening delimiter for the regular expression is supplied; the
 * end of it is marked by an unescaped matching delimiter or, if
 * delim_only is FALSE, by a '\0' character. We return a pointer to
 * the terminating '\0' or to the character following the closing
 * delimiter, or NULL if we failed.
 *
 * If, after we've found a delimiter, we have an empty pattern string,
 * we use the last compiled expression if there is one.
 *
 * The regular expression is converted to egrep syntax by mapstring(),
 * which also finds the closing delimiter. The actual compilation is
 * done by regcomp(), from Henry Spencer's regexp routines.
 *
 * If we're successful, the compiled regular expression will be
 * pointed to by lastprogp->rn_ptr, & lastprogp->rn_count will be > 0.
 */
static char *
compile(pat, delimiter, delim_only)
char	*pat;
int	delimiter;
bool_t	delim_only;
{
    Rnode	*progp;

    if (pat == NULL) {
	return(NULL);
    }

    /*
     * If we get an empty regular expression, we just use the last
     * one we compiled (if there was one).
     */
    if (*pat == '\0') {
	return((delim_only || lastprogp == NULL) ? NULL : pat);
    }
    if (*pat == delimiter) {
	return((lastprogp == NULL) ? NULL : &pat[1]);
    }

    progp = rn_new(mapstring(&pat, delimiter));
    if (progp == NULL) {
	return(NULL);
    }

    if (*pat == '\0') {
	if (delim_only) {
	    rn_delete(progp);
	    return(NULL);
	}
    } else {
	pat++;
    }
    rn_delete(lastprogp);
    lastprogp = progp;
    return(pat);
}

/*
 * Search from the specified position in the specified direction.
 *
 * If the pattern is found, return a pointer to its Posn, otherwise NULL.
 */
Posn *
search(startline, startindex, dir, strp)
Line		*startline;	/* Where to start searching from */
int		startindex;	/* Where to start searching from */
int		dir;		/* FORWARD or BACKWARD */
char		**strp;		/* Pointer to pattern without initial / or ? */
{
    Posn	*pos;
    Posn	*(*sfunc) P((Line *, int, bool_t));
    char	*str;

    str = compile(*strp, (dir == FORWARD) ? '/' : '?', FALSE);
    if (str == NULL) {
	return(NULL);
    }
    *strp = str;

    if (dir == BACKWARD) {
	sfunc = bcksearch;
    } else {
	sfunc = fwdsearch;
    }
    pos = (*sfunc)(startline, startindex, Pb(P_wrapscan));

    return(pos);
}

/*
 * Search for the given expression, ignoring regextype, without
 * wrapscan & and without using the compiled regular expression for
 * anything else (so 'n', 'N', etc., aren't affected). We do, however,
 * cache the compiled form for the last string we were given.
 */
Posn *
xvFindPattern(startpos, str, dir, match_curpos)
Posn		*startpos;
char		*str;
int		dir;
bool_t		match_curpos;
{
    static Rnode	*progp = NULL;
    static char		*last_str = NULL;
    Rnode		*old_progp;
    Posn		*pos;
    Posn		*(*sfunc) P((Line *, int, bool_t));

    if (str == NULL) {
	return(NULL);
    }
    if (str != last_str &&
	(last_str == NULL || strcmp(str, last_str) != 0)) {
	if (progp) {
	    rn_delete(progp);
	}
	progp = rn_new(str);
	last_str = str;
    }
    if (progp == NULL) {
	last_str = NULL;
	return(NULL);
    }

    if (dir == BACKWARD) {
	sfunc = bcksearch;
    } else {
	sfunc = fwdsearch;
    }

    old_progp = lastprogp;

    lastprogp = progp;

    /*
     * If match_curpos is TRUE, we need to test the current position for
     * a match before doing the search. If it fails, then we do the search
     * anyway. The logic here is not very obvious.
     */
    if (
	!match_curpos
	||
	(pos = match(startpos->p_line, startpos->p_index)) == NULL
    ) {
	pos = (*sfunc)(startpos->p_line, startpos->p_index, FALSE);
    }

    lastprogp = old_progp;

    return(pos);
}

/*
 * Perform line-based search, returning a pointer to the first line
 * (forwards or backwards) on which a match is found, or NULL if there
 * is none in the buffer specified.
 */
Line *
linesearch(startline, dir, strp)
Line	*startline;	/* Where to start searching from */
int	dir;		/* FORWARDS or BACKWARDS */
char	**strp;		/* The search pattern without the initial / or ? */
{
    Posn	pos;
    Posn	*newpos;

    pos.p_line = startline;
    pos.p_index = 0;
    if (dir == FORWARD) {
	/*
	 * We don't want a match to occur on the current line,
	 * but setting the starting position to the next line
	 * is wrong because we will not match a pattern at the
	 * start of the line. So go to the end of this line.
	 */
	if (gchar(&pos) != '\0') {
	    while (inc(&pos) == mv_SAMELINE) {
		;
	    }
	}
    }

    newpos = search(pos.p_line, pos.p_index, dir, strp);
    return((newpos != NULL) ? newpos->p_line : NULL);
}

/*
 * regerror - called by regexp routines when errors are detected.
 */
void
regerror(s)
char	*s;
{
    if (echo & e_REGERR) {
	show_error("%s", s);
    }
    echo &= ~(e_REGERR | e_NOMATCH);
}

/* Static data for the return value of match() and rmatch() */
static Posn	matchposn;

/*
 * Find a match at or after "ind" on the given "line"; return
 * pointer to Posn of match, or NULL if no match was found.
 */
static Posn *
match(line, ind)
Line	*line;
int	ind;
{
    char	*s;
    regexp	*prog;

    s = line->l_text + ind;
    prog = cur_prog();

    if (regexec(prog, s, (ind == 0))) {
	int	llen;

	matchposn.p_line = line;
	matchposn.p_index = (int) (prog->startp[0] - line->l_text);

	/*
	 * If the match is after the end of the line,
	 * move it to the last character of the line,
	 * unless the line has no characters at all.
	 */
	llen = strlen(line->l_text);
	if (matchposn.p_index >= llen) {
	    matchposn.p_index = (llen > 0 ? llen - 1 : 0);
	}

	return(&matchposn);
    } else {
	return(NULL);
    }
}

/*
 * Like match(), but returns the last available match on the given
 * line which is before the index given in maxindex.
 */
static Posn *
rmatch(line, ind, maxindex)
Line		*line;
register int	ind;
int		maxindex;
{
    register int	lastindex = -1;
    Posn		*pos;
    register char	*ltp;

    ltp = line->l_text;
    for (; (pos = match(line, ind)) != NULL; ind++) {
	ind = pos->p_index;
	if (ind >= maxindex)
	    break;
	/*
	 * If we've found a match on the last
	 * character of the line, return it here or
	 * we could get into an infinite loop.
	 */
	if (ltp[lastindex = ind] == '\0' || ltp[ind + 1] == '\0')
	    break;
    }

    if (lastindex >= 0) {
	matchposn.p_index = lastindex;
	matchposn.p_line = line;
	return &matchposn;
    } else {
	return NULL;
    }
}

/*
 * Search forwards through the buffer for a match of the last
 * pattern compiled.
 */
static Posn *
fwdsearch(startline, startindex, wrapscan)
Line		*startline;
int		startindex;
bool_t		wrapscan;
{
    static Posn	*pos;		/* location of found string */
    Line	*lp;		/* current line */
    Line	*last;

    last = curbuf->b_lastline;

    /*
     * First, search for a match on the current line after the cursor
     * position (only if the current line is non-empty).
     */
    if (startline->l_text[0] != '\0') {
	pos = match(startline, startindex + 1);
	if (pos != NULL) {
	    return(pos);
	}
    }

    /*
     * Now search all the lines from here to the end of the file,
     * and from the start of the file back to here if (wrapscan).
     */
    for (lp = startline->l_next; lp != startline; lp = lp->l_next) {
	/*
	 * Wrap around to the start of the file.
	 */
	if (lp == last) {
	    if (wrapscan) {
		lp = curbuf->b_line0;
		continue;
	    } else {
		return(NULL);
	    }
	}

	pos = match(lp, 0);
	if (pos != NULL) {
	    return(pos);
	}
    }

    /*
     * Finally, search from the start of the cursor line
     * up to the cursor position. (Wrapscan was set if
     * we got here.)
     */
    pos = match(startline, 0);
    if (pos != NULL) {
	if (pos->p_index <= startindex) {
	    return(pos);
	}
    }

    return(NULL);
}

/*
 * Search backwards through the buffer for a match of the last
 * pattern compiled.
 *
 * If the pattern is found, return a pointer to its Posn, otherwise NULL.
 *
 * Because we're searching backwards, we have to return the
 * last match on a line if there is more than one, so we call
 * rmatch() instead of match().
 */
static Posn *
bcksearch(startline, startindex, wrapscan)
Line		*startline;
int		startindex;
bool_t		wrapscan;
{
    Posn	*pos;		/* location of found string */
    Line	*lp;		/* current line */
    Line	*line0;

    /*
     * First, search for a match on the current line before the
     * current cursor position; if "begword" is set, it must be
     * before the current cursor position minus one.
     */
    pos = rmatch(startline, 0, startindex);
    if (pos != NULL) {
	return(pos);
    }

    /*
     * Search all lines back to the start of the buffer,
     * and then from the end of the buffer back to the
     * line after the cursor line if wrapscan is set.
     */
    line0 = curbuf->b_line0;
    for (lp = startline->l_prev; lp != startline; lp = lp->l_prev) {

	if (lp == line0) {
	    if (wrapscan) {
		/*
		 * Note we do a continue here so that
		 * the loop control works properly.
		 */
		lp = curbuf->b_lastline;
		continue;
	    } else {
		return(NULL);
	    }
	}
	pos = rmatch(lp, 0, INT_MAX);
	if (pos != NULL)
	    return pos;
    }

    /*
     * Finally, try for a match on the cursor line
     * after (or at) the cursor position.
     */
    pos = rmatch(startline, startindex, INT_MAX);
    if (pos != NULL) {
	return(pos);
    }

    return(NULL);
}

/*
 * Execute a global command of the form:
 *
 * g/pattern/X
 *
 * where 'x' is a command character, currently one of the following:
 *
 * d	Delete all matching lines
 * l	List all matching lines
 * p	Print all matching lines
 * s	Perform substitution
 * &	Repeat last substitution
 * ~	Apply last right-hand side used in a substitution to last
 *	regular expression used
 *
 * The command character (as well as the trailing slash) is optional, and
 * is assumed to be 'p' if missing.
 *
 * The "lp" and "up" parameters are the first line to be considered, and
 * the last line to be considered. If these are NULL, the whole buffer is
 * considered; if only up is NULL, we consider the single line "lp".
 *
 * The "forward" parameter is TRUE if we are doing 'g', FALSE for 'v'.
 */
bool_t
exGlobal(lp, up, cmd, forward)
Line		*lp, *up;
char		*cmd;
bool_t		forward;
{
    Rnode		*globprogp;
    regexp		*prog;		/* compiled pattern */
    long		ndone;		/* number of matches */
    register char	cmdchar = '\0';	/* what to do with matching lines */

    /* Skip blanks between the g and the delimiter */
    while (*cmd != '\0' && is_space(*cmd)) cmd++;

    /*
     * compile() compiles the pattern up to the first unescaped
     * delimiter: we place the character after the delimiter in
     * cmdchar. If there is no such character, we default to 'p'.
     */
    if (*cmd == '\0' || (cmd = compile(&cmd[1], *cmd, FALSE)) == NULL) {
	regerror(forward ?
		"Usage: :g/search pattern/command" :
		"Usage: :v/search pattern/command");
	return(FALSE);
    }
    /*
     * Check we can do the command before starting.
     */
    switch (cmdchar = *cmd) {
    case '\0':
	cmdchar = 'p';
	 /* fall through ... */
    case 'l':
    case 'p':
	break;
    case 's':
    case '&':
    case '~':
	cmd++;	/* cmd points at char after modifier */
	 /* fall through ... */
    case 'd':
	if (!start_command(NULL)) {
	    return(FALSE);
	}
	break;
    default:
	regerror("Invalid command character");
	return(FALSE);
    }

    ndone = 0;

    /*
     * If no range was given, do every line.
     * If only one line was given, just do that one.
     * Ensure that "up" points at the line after the
     * last one in the range, to make the loop easier.
     */
    if (lp == NULL) {
	lp = curbuf->b_file;
	up = curbuf->b_lastline;
    } else if (up == NULL) {
	up = lp->l_next;
    } else {
	up = up->l_next;
    }

    /*
     * If we are going to print lines, it is sensible
     * to find out the line number of the first line in
     * the range before we start, and increment it as
     * we go rather than finding out the line number
     * for each line as it is printed.
     */
    switch (cmdchar) {
    case 'p':
    case 'l':
	curnum = lineno(lp);
	curline = lp;
	lastline = up;
	greptype = forward;
	disp_init(grep_line, (int) curwin->w_ncols, (cmdchar == 'l'));
	return(TRUE);
    }

    /*
     * This is tricky. exSubstitute() might default to
     * using cur_prog(), if the command is of the form
     *
     *	:g/pattern/s//.../
     *
     * so cur_prog() must still reference the expression we
     * compiled. On the other hand, it may compile a
     * different regular expression, so we have to be able
     * to maintain a separate one (which is what globprogp
     * is for). Moreover, if it does compile a different
     * expression, one of them has to be freed afterwards.
     *
     * This is why we use Rnodes, which contain
     * reference counts. An Rnode, & the compiled
     * expression it points to, are only freed when its
     * reference count is decremented to 0.
     */
    globprogp = rn_duplicate(lastprogp);
    prog = cur_prog();

    /*
     * Place the cursor at bottom left of the window,
     * so the user knows what we are doing.
     * It is safe not to put the cursor back, because
     * we are going to produce some more output anyway.
     */
    gotocmd(FALSE);

    /*
     * Try every line from lp up to (but not including) up.
     */
    ndone = 0;
    while (lp != up) {
	if (forward == regexec(prog, lp->l_text, TRUE)) {
	    Line	*thisline;

	    /*
	     * Move the cursor to the line before
	     * doing anything. Also move the line
	     * pointer on one before calling any
	     * functions which might alter or delete
	     * the line.
	     */
	    move_cursor(lp, 0);

	    thisline = lp;
	    lp = lp->l_next;

	    switch (cmdchar) {
	    case 'd':	/* delete the line */
		repllines(thisline, 1L,
			(Line *) NULL);
		ndone++;
		break;
	    case 's':	/* perform substitution */
	    case '&':
	    case '~':
	    {
		register long	(*func) P((Line *, Line *, char *));
		unsigned	savecho;

		switch (cmdchar) {
		case 's':
		    func = exSubstitute;
		    break;
		case '&':
		    func = exAmpersand;
		    break;
		case '~':
		    func = exTilde;
		}

		savecho = echo;

		echo &= ~e_NOMATCH;
		ndone += (*func) (thisline, thisline, cmd);

		echo = savecho;
		break;
	    }
	    }
	} else {
	    lp = lp->l_next;
	}
    }

    /*
     * If globprogp is still the current prog, this should just
     * decrement its reference count to 1: otherwise, if
     * exSubstitute() has compiled a different pattern, then that
     * counts as the last compiled pattern, globprogp's reference
     * count should be decremented to 0, & it should be freed.
     */
    rn_delete(globprogp);

    switch (cmdchar) {
    case 'd':
    case 's':
    case '&':
    case '~':
	end_command();
	if (ndone) {
	    xvUpdateAllBufferWindows();
	    cursupdate();
	    begin_line(TRUE);
	    if (ndone >= Pn(P_report)) {
		show_message(
			 (cmdchar == 'd') ?
			 "%ld fewer line%c" :
			 "%ld substitution%c",
			 ndone,
			 (ndone > 1) ?
			 's' : ' ');
	    }
	}
    }

    if (ndone == 0 && (echo & e_NOMATCH)) {
	regerror("No match");
	return(FALSE);
    }

   return(TRUE);
}

static char *
grep_line()
{
    static Flexbuf	b;
    regexp		*prog;

    prog = cur_prog();
    for ( ; curline != lastline; curline = curline->l_next, curnum++) {

	if (greptype == regexec(prog, curline->l_text, TRUE)) {

	    flexclear(&b);
	    if (Pb(P_number)) {
		(void) lformat(&b, NUM_FMT, curnum);
	    }
	    (void) lformat(&b, "%s", curline->l_text);
	    break;
	}
    }

    if (curline == lastline) {
	return(NULL);
    } else {
	curline = curline->l_next;
	curnum++;
	return(flexgetstr(&b));
    }
}

/*
 * regsubst - perform substitutions after a regexp match
 *
 * Adapted from a routine from Henry Spencer's regexp package. The
 * original copyright notice for all these routines is in regexp.c,
 * which is distributed herewith.
 */

#ifndef CHARBITS
#	define	UCHARAT(p)	((int)*(unsigned char *)(p))
#else
#	define	UCHARAT(p)	((int)*(p)&CHARBITS)
#endif

static void
regsubst(prog, src, dest, lnum)
register regexp	*prog;
register char	*src;
Flexbuf		*dest;
unsigned long	lnum;
{
    register int	c;
    register char	ul;
    register int	rxtype;     /* can be rt_TAGS, rt_GREP or rt_EGREP */

    if (prog == NULL || src == NULL || dest == NULL) {
	regerror("NULL parameter to regsubst");
	return;
    }

    if (UCHARAT(prog->program) != MAGIC) {
	regerror("Damaged regexp fed to regsubst");
	return;
    }

    /*
     * This controls whether we are converting the case of letters
     * to upper or lower case. It only has any effect if set to [UuLl].
     */
    ul = '\0';

    rxtype = Pn(P_regextype);

    while ((c = *src++) != '\0') {
	register int	no = -1;

	/*
	 * First check for metacharacters. Note that if regextype is "tags", we
	 * don't treat & (or ~) as being special. This matches the behaviour of
	 * "nomagic" in vi (for which regextype=tags should be synonymous).
	 */
	if (c == '&' && rxtype != rt_TAGS) {
	    no = 0;
	} else if (c == '\\') {
	    switch (*src) {
	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9':
		no = *src++ - '0';
		break;
	    case '#':
		(void) lformat(dest, NUM_FMT, lnum);
		src++;
		continue;
	    case 'u': case 'U':
	    case 'l': case 'L':
	    case 'e': case 'E':
		ul = *src++;
		continue;
	    default:
		break;
	    }
	}

	if (no < 0) {
	    /*
	     * It's an ordinary character.
	     */
	    if (c == '\\' && *src != '\0') {
		c = *src++;
	    }

	    add_char_to_rhs(dest, c, ul);

	    if (ul == 'u' || ul == 'l') {
		ul = '\0';
	    }

	} else if (prog->startp[no] != NULL && prog->endp[no] != NULL) {
	    register char	*cp;

	    /*
	     * It isn't an ordinary character, but a reference
	     * to a string matched on the lhs. Notice that we
	     * just do nothing if we find a reference to a null
	     * match, or one that doesn't exist; we can't tell
	     * the difference at this stage.
	     */
	    for (cp = prog->startp[no]; cp < prog->endp[no]; cp++) {
		if (*cp == '\0') {
		    regerror("Damaged match string");
		    return;
		} else {
		    add_char_to_rhs(dest, *cp, ul);

		    /* \u and \l only change the case of the first character
		     * of a replacement string and if the replacement string is
		     * empty, they change the first character of what follows.
		     */
		    if (ul == 'u' || ul == 'l') {
			ul = '\0';
		    }
		}
	    }
	}
    }
}

static void
add_char_to_rhs(dest, c, ulmode)
register Flexbuf	*dest;
register int		c;
register int		ulmode;
{
    switch (ulmode) {
    case 'u': case 'U':
	if (is_lower(c)) {
	    c = to_upper(c);
	}
	break;

    case 'l': case 'L':
	if (is_upper(c)) {
	    c = to_lower(c);
	}
    }

    (void) flexaddch(dest, c);
}

/*
 * exSubstitute(lp, up, cmd)
 *
 * Perform a substitution from line 'lp' up to (but not including)
 * line 'up' using the command pointed to by 'cmd' which should be
 * of the form:
 *
 * /pattern/substitution/g
 *
 * The trailing 'g' is optional and, if present, indicates that multiple
 * substitutions should be performed on each line, if applicable.
 * The usual escapes are supported as described in the regexp docs.
 *
 * The return value is the number of lines on which substitutions occurred,
 * 0 if there were no matches or an error.
 */
long
exSubstitute(lp, up, command)
Line	*lp, *up;
char	*command;
{
    char	*copy;		/* for copy of command */
    char	*sub;
    char	*cp;
    char	delimiter;
    long	nsubs;
    bool_t	magic;		/* Is the "magic" parameter set? */

    /* Skip blanks between the s and the delimiter */
    while (*command != '\0' && is_space(*command)) command++;

    /* ":s" means repeat the last substitution */
    if (*command == '\0') {
	return(exAmpersand(lp, up, command));
    }

    copy = strsave(command);
    if (copy == NULL) {
	return(0);
    }

    delimiter = *copy;
    if (delimiter == '\0' ||
			(cp = compile(&copy[1], delimiter, TRUE)) == NULL) {
	regerror("Usage: :s/search pattern/replacement/");
	free(copy);
	return(0);
    }
    sub = cp;

    /*
     * Scan past the rhs to the flags, if any.
     */
    for (; *cp != '\0'; cp++) {
	if (*cp == '\\') {
	    if (*++cp == '\0') {
		break;
	    }
	} else if (*cp == delimiter) {
	    *cp++ = '\0';
	    break;
	}
    }

    /*
     * Save the regular expression for exAmpersand().
     */
    if (last_lhs) {
	rn_delete(last_lhs);
    }
    last_lhs = rn_duplicate(lastprogp);

    magic = (Pn(P_regextype) != rt_TAGS);

    /*
     * Check for "%" or "~", which both mean "the last substituted text"
     * In nomagic mode, \~ is expanded but \% is not.
     */
    if ((magic && strcmp(sub, "%") == 0) ||
        strcmp(sub, magic ? "~" : "\\~") == 0) {
	sub = last_rhs;
    } else if (strchr(sub, '~') != NULL) {
	bool_t escaped_next;

	/* If sub contains ~ characters, we need to expand them here
	 * so that another successive use of ~ expands to the previous RHS
	 * including the expanded version of any ~s used then, so on a line
	 * "one one one",
	 * :s/one/two/
	 * :s/one/~~/
	 * :s/one/~~/	gives
	 * two twotwo twotwotwotwo
	 */
	Flexbuf newsub;
	flexnew(&newsub);

	escaped_next = FALSE;
	for (cp = sub; *cp != '\0'; cp++) {
	    switch (*cp) {
	    case '\\':
		if (escaped_next) {		/* \\ */
		    flexaddch(&newsub, *cp);
		    flexaddch(&newsub, *cp);
		    escaped_next = FALSE;
		} else {
		    if (cp[1] == '\0') {	/* \ at end of line */
			flexaddch(&newsub, *cp);
		    } else {
			escaped_next = TRUE;
		    }
		}
		break;

	    case '~':
		/*
		 * In magic mode, unescaped tildes are replaced with last RHS;
		 * In nomagic mode, escaped tildes are replaced with last RHS.
		 */
		if (magic ^ escaped_next) {
		    lformat(&newsub, "%s", last_rhs);
		} else {
		    if (escaped_next) flexaddch(&newsub, '\\');
		    flexaddch(&newsub, *cp);
		}
		escaped_next = FALSE;
		break;

	    default:
		flexaddch(&newsub, *cp);
		escaped_next = FALSE;
		break;
	    }
	}
	free(copy);
	sub = flexdetach(&newsub);
	copy = sub;	/* What to free instead */
    }

    nsubs = substitute(lp, up, sub, cp);

    /*
     * Save the rhs.
     */
    if (sub != last_rhs) {
	free(last_rhs);
        last_rhs = strsave(sub);
    }

    free(copy);

    return(nsubs);
}

/*
 * Repeat last substitution.
 *
 * For vi compatibility, this also changes the value of the last
 * regular expression used.
 */
long
exAmpersand(lp, up, flags)
Line	*lp, *up;
char	*flags;
{
    long	nsubs;

    if (last_lhs == NULL || last_rhs == NULL) {
	show_error("No substitute to repeat!");
	return(0);
    }
    rn_delete(lastprogp);
    lastprogp = rn_duplicate(last_lhs);
    nsubs = substitute(lp, up, last_rhs, flags);
    return(nsubs);
}

/*
 * Apply last right-hand side used in a substitution to last regular
 * expression used.
 *
 * For vi compatibility, this also changes the value of the last
 * substitution.
 */
long
exTilde(lp, up, flags)
Line	*lp, *up;
char	*flags;
{
    long	nsubs;

    if (lastprogp == NULL || last_rhs == NULL) {
	show_error("No substitute to repeat!");
	return(0);
    }
    if (last_lhs) {
	rn_delete(last_lhs);
    }
    last_lhs = rn_duplicate(lastprogp);
    nsubs = substitute(lp, up, last_rhs, flags);
    return(nsubs);
}

static long
substitute(lp, up, sub, flags)
Line	*lp, *up;
char	*sub;
char	*flags;
{
    long	nsubs;
    Flexbuf	ns;
    regexp	*prog;
    bool_t	do_all;		/* true if 'g' was specified */
    Line	*lp0;

    if (!start_command(NULL)) {
	return(0);
    }

    prog = cur_prog();

    do_all = (*flags == 'g');

    nsubs = 0;
    lp0 = curwin->w_cursor->p_line;

    /*
     * If no range was given, do the current line.
     * If only one line was given, just do that one.
     * Ensure that "up" points at the line after the
     * last one in the range, to make the loop easier.
     */
    if (lp == NULL) {
	lp = lp0;
    }
    if (up == NULL) {
	up = lp->l_next;
    } else {
	up = up->l_next;
    }
    flexnew(&ns);
    for (; lp != up; lp = lp->l_next) {
	if (regexec(prog, lp->l_text, TRUE)) {
	    char	*p, *matchp;

	    /*
	     * Save the line that was last changed for the final
	     * cursor position (just like the real vi).
	     */
	    if (lp0 != NULL && lp != lp0) {
		setpcmark();
		lp0 = NULL;
	    }
	    move_cursor(lp, 0);

	    flexclear(&ns);
	    p = lp->l_text;

	    do {
		/*
		 * Copy up to the part that matched.
		 */
		while (p < prog->startp[0]) {
		    (void) flexaddch(&ns, *p);
		    p++;
		}

		regsubst(prog, sub, &ns, lineno(lp));

		/*
		 * Continue searching after the match.
		 *
		 * Watch out for null matches - we
		 * don't want to go into an endless
		 * loop here.
		 */
		matchp = p = prog->endp[0];
		if (prog->startp[0] >= p) {
		    if (*p == '\0') {
			/*
			 * End of the line.
			 */
			break;
		    } else {
			matchp++;
		    }
		}

	    } while (do_all && regexec(prog, matchp, FALSE));

	    /*
	     * Copy the rest of the line, that didn't match.
	     */
	    (void) lformat(&ns, "%s", p);
	    replchars(lp, 0, strlen(lp->l_text),
		  flexgetstr(&ns));
	    nsubs++;
	}
    }
    flexdelete(&ns);			/* free the temp buffer */
    end_command();

    if (!nsubs && (echo & e_NOMATCH)) {
	regerror("No match");
    }
    return(nsubs);
}
