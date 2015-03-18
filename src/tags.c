/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    tags.c
* module function:
    Handle tags.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

#define	LSIZE		512	/* max. size of a line in the tags file */

/*
 * Macro evaluates true if char 'c' is a valid identifier character.
 * Used by tagword().
 */
#define IDCHAR(c)	(is_alnum(c) || (c) == '_')

/*
 * New dynamic tags stuff.
 */
static	TAG	**hashtable = NULL;
static	int	hashtabsize = 1009;	/* should be a prime; e.g. 2003 is ok */

static	void	EnterTag P((char *));
static	void	tagFree P((void));

void
tagInit()
{
    char		**tagfiles;
    int			count;
    char		inbuf[LSIZE];
    register FILE	*fp;
    register char	*cp;
    register int	c;

    if (hashtable != NULL) {
	return;			/* Already initialised */
    } else {
	/*
	 * Using calloc() avoids having to clear memory.
	 */
	hashtable = (TAG **) calloc(hashtabsize, sizeof(TAG *));
	if (hashtable == NULL) {
	    return;
	}
    }

    tagfiles = Pl(P_tags);
    if (tagfiles == NULL) {
	return;
    }

    for (count = 0; tagfiles[count] != NULL; count++) {

	fp = fopen(fexpand(tagfiles[count], FALSE), "r");
	if (fp == NULL) {
	    continue;
	}

	while (TRUE) {
	    register int n;
	    /*
	     * Read the whole line into inbuf.
	     */
	    cp = inbuf; n = 0;
	    while ((c = getc(fp)) != EOF && c != '\n'
		   && ++n < sizeof(inbuf)) {
		*cp++ = c;
	    }
	    if (c == EOF) {
		break;
	    }
	    *cp = '\0';

	    /*
	     * Enter the tag into the hash table.
	     */
	    EnterTag(inbuf);
	}
	(void) fclose(fp);
    }
}

/*
 * Tag to the word under the cursor.
 */
void
tagword()
{
    char	ch;
    Posn	pos;
    char	tagbuf[50];
    char	*cp = tagbuf;

    pos = *curwin->w_cursor;

    /*
     * Advance past any whitespace to get to a taggable word.
     */
    ch = gchar(&pos);
    while (is_space(ch)) {
	if (inc(&pos) != mv_SAMELINE) {
	    break;
	}
	ch = gchar(&pos);
    }
    if (!IDCHAR(ch)) {
	return;
    }

    /*
     * Now grab the chars in the identifier.
     */
    while (IDCHAR(ch) && cp < tagbuf + sizeof(tagbuf)) {
	*cp++ = ch;
	if (inc(&pos) != mv_SAMELINE)
	    break;
	ch = gchar(&pos);
    }

    /*
     * If the identifier is too long, just beep.
     */
    if (cp >= tagbuf + sizeof(tagbuf)) {
	beep(curwin);
	return;
    }

    *cp = '\0';

    (void) exTag(curwin, tagbuf, FALSE, TRUE, TRUE);
}

/*
 * exTag(window, tag, force, interactive, split) - goto tag
 */
bool_t
exTag(window, tag, force, interactive, split)
Xviwin	*window;
char	*tag;			/* function to search for */
bool_t	force;			/* if true, force re-edit */
bool_t	interactive;		/* true if reading from tty */
bool_t	split;			/* true if want to split */
{
    static char	last_tag[32];	/* record of last tag we were given */
    bool_t	edited;		/* TRUE if we have edited the file */
    Xviwin	*tagwindow;	/* tmp window pointer */
    TAG		*tp;
    int		l1, l2;

    /*
     * Re-use previous tag if none given; if one is given, remember it.
     */
    if (tag == NULL || tag[0] == '\0') {
    	tag = last_tag;
    } else {
	(void) strncpy(last_tag, tag, sizeof(last_tag));
    }

    if (tag == NULL || tag[0] == '\0') {
	if (interactive) {
	    show_error(window, "Usage: :tag <identifier>");
	} else {
	    beep(window);
	}
	return(FALSE);
    }

    gotocmd(window, FALSE);

    tp = tagLookup(tag, &l1, &l2);
    if (tp == NULL) {
	if (interactive) {
	    show_error(window, "Tag not found");
	}
	return(FALSE);
    }

    /*
     * If we are already editing the file, just do the search.
     *
     * If "autosplit" is set, create a new buffer window and
     * edit the file in it.
     *
     * Else just edit it in the current window.
     */
    tagwindow = xvFindWindowByName(window, tp->t_file);
    if (tagwindow != NULL) {
	curwin = tagwindow;
	curbuf = curwin->w_buffer;
	edited = TRUE;

    } else if (split && xvCanSplit(window) &&
				exNewBuffer(window, tp->t_file, 0)) {
	edited = TRUE;

    } else if (exEditFile(window, force, tp->t_file)) {
	edited = TRUE;

    } else {
	/*
	 * If the re-edit failed, abort here.
	 */
	edited = FALSE;
    }

    /*
     * Finally, search for the given pattern in the file,
     * but only if we successfully edited it. Note that we
     * always use curwin at this stage because it is not
     * necessarily the same as our "window" parameter.
     */
    if (edited) {
	int	old_rxtype;
	Posn	*p;
	char	*pattern;
	char	*cp;

	switch (tp->t_locator[0]) {
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	    /*
	     * Not a search pattern; a line number.
	     */
	    xvMoveToLineNumber(atol(tp->t_locator));
	    break;

	case '/':
	case '?':
	    pattern = strsave(tp->t_locator + 1);
	    if (pattern == NULL) {
		break;			/* What else can we do? */
	    }

	    /*
	     * Remove trailing '/' or '?' from the search pattern.
	     */
	    cp = pattern + strlen(pattern) - 1;
	    if (*cp == '/' || *cp == '?') {
		*cp = '\0';
	    }

	    /*
	     * Set the regular expression type to rt_TAGS
	     * so that only ^ and $ have a special meaning;
	     * this is like nomagic in "real" vi.
	     */
	    old_rxtype = Pn(P_regextype);
	    set_param(P_regextype, rt_TAGS, (char **) NULL);

	    p = xvDoSearch(curwin, pattern, '/');
	    free(pattern);
	    if (p != NULL) {
		setpcmark(curwin);
		move_cursor(curwin, p->p_line, p->p_index);
		curwin->w_set_want_col = TRUE;
		show_file_info(curwin, TRUE);
	    } else {
		beep(curwin);
	    }
	    set_param(P_regextype, old_rxtype, (char **) NULL);
	    break;

	default:
	    show_error(curwin, "Ill-formed tag pattern \"%s\"", tp->t_locator);
	}
	move_window_to_cursor(curwin);
	redraw_all(curwin, FALSE);
    }

    return(TRUE);
}

static void
EnterTag(line)
char	*line;
{
    register TAG	*tp;
    register TAG	**tpp;
    register char	*cp;
    register unsigned	f;
    register int	max_chars;

    max_chars = Pn(P_taglength);
    if (max_chars == 0) {
	max_chars = INT_MAX;
    }

    /*
     * Exuberant ctags creates tags with extra stuff on the end after ;" like
ANY	regexp.c	95;"	d	file:
Ev_resize	virtscr.h	/^	Ev_resize,$/;"	e	enum:xvevent::__anon25
     * One way would be to implement ;-separated multiple commands and " comments
     * A simpler one is to strip this here.
     */
    { /* Match ;"\t and delete it if found */
	char *p,*q;
        if ((p = strrchr(line, ';')) != NULL && (q = strrchr(line, '"')) != NULL
	    && q == p+1 && q[1] == '\t') *p = '\0';
    }

    /*
     * Allocate space for the structure immediately followed by
     * the text. Then copy the string into the allocated space
     * and set up the pointers within the structure.
     */
    tp = (TAG *) malloc(sizeof(TAG) + strlen(line) + 1);
    if (tp == NULL) {
	return;
    }
    tp->t_name = (char *) (tp + 1);
    (void) strcpy(tp->t_name, line);
    tp->t_file = strchr(tp->t_name, '\t');
    if (tp->t_file == NULL) {
	tp->t_file = "No File!";
    } else {
	/*
	 * Limit tag name length if necessary.
	 */
	if (tp->t_file > (tp->t_name + max_chars)) {
	    tp->t_name[max_chars] = '\0';
	} else {
	    *(tp->t_file) = '\0';
	}
	tp->t_file++;
    }
    tp->t_locator = strchr(tp->t_file, '\t');
    if (tp->t_locator == NULL) {
	tp->t_locator = "0";
    } else {
	*(tp->t_locator++) = '\0';
    }

    /*
     * Calculate hash value.
     */
    f = 0;
    for (cp = tp->t_name; *cp != '\0'; cp++) {
	f <<= 1;
	f ^= *cp;
    }

    tpp = &hashtable[f % hashtabsize];
    tp->t_next = *tpp;
    *tpp = tp;
}

/*
 * Look up the given name to see if it is a valid tag. The string
 * might not be null-terminated, so we only look at characters which
 * are considered valid in a tag.
 *
 * *lenptr is set to the length of the valid tag found.
 *
 * *offsetptr is set to the number of punctuation/whitespace
 * characters until the start of the next valid tag.
 */
TAG *
tagLookup(name, lenptr, offsetptr)
char	*name;
int	*lenptr;
int	*offsetptr;
{
    register TAG	*tp;
    register char	*cp;
    register unsigned	f;
    register int	length;
    int			max_chars;

    /*
     * Check that the tag cache is loaded; if not, load it now.
     */
    if (hashtable == NULL) {
	tagInit();
	if (hashtable == NULL) {
	    return(NULL);
	}
    }

    if (!IDCHAR(*name)) {
	/*
	 * No possibility of a valid tag: return the number of
	 * non-identifier characters before we reach an identifier.
	 */
	length = 0;
	for (cp = name; *cp != '\0' && !IDCHAR(*cp); cp++) {
	    length++;
	}
	*offsetptr = length;
	return(NULL);
    }

    max_chars = Pn(P_taglength);
    if (max_chars == 0) {
	max_chars = INT_MAX;
    }

    /*
     * Calculate hash value.
     */
    f = 0;
    for (cp = name, length = 0;
			*cp != '\0' && IDCHAR(*cp) && length < max_chars;
			cp++, length++) {
	
	f <<= 1;
	f ^= *cp;
    }
    *lenptr = length;

    /*
     * Skip over remaining identifier characters.
     */
    for (*offsetptr = length; *cp != '\0' && IDCHAR(*cp); cp++) {
	(*offsetptr)++;
    }

    for (tp = hashtable[f % hashtabsize]; tp != NULL; tp = tp->t_next) {
	/*
	 * Match if strings are identical up to the tag length, AND
	 * the stored tag string is not longer than that length.
	 */
    	if (strncmp(name, tp->t_name, length) == 0 &&
					strlen(tp->t_name) <= length) {
	    return(tp);
	}
    }
    return(NULL);
}

/*
 * Free all memory used by the tag cache, and set hashtable to NULL.
 * This causes a complete reload next time a tag lookup is performed.
 */
static void
tagFree()
{
    int		count;

    if (hashtable == NULL) {
	return;
    }

    for (count = 0; count < hashtabsize; count++) {
	TAG	*tp, *tmp;

	for (tp = hashtable[count]; tp != NULL; ) {
	    tmp = tp;
	    tp = tp->t_next;
	    free((genptr *) tmp);
	}
    }
    free((genptr *) hashtable);
    hashtable = NULL;
}

/*
 * If someone sets a tags-related parameter, we just invalidate the
 * entire tag cache, causing an automatic reload at next reference.
 */
/*ARGSUSED*/
bool_t
tagSetParam(win, new_value, interactive)
Xviwin		*win;
Paramval	new_value;
bool_t		interactive;
{
    tagFree();
    return(TRUE);
}
