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

    tagfiles = Pl(P_tags);
    if (tagfiles == NULL) {
	return;
    }

    if (hashtable != NULL) {
	return;			/* Already initialised */
    }

    for (count = 0; tagfiles[count] != NULL; count++) {

	fp = fopen(fexpand(tagfiles[count], FALSE), "r");
	if (fp == NULL) {
	    continue;
	}

	if (hashtable == NULL) {
	    /*
	     * Using calloc() avoids having to clear memory.
	     */
	    hashtable = clr_alloc(hashtabsize, sizeof(TAG *));
	    if (hashtable == NULL) {
		return;
	    }
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
	beep();
	return;
    }

    *cp = '\0';

    (void) exTag(tagbuf, FALSE, TRUE, TRUE);
}

/*
 * exTag(window, tag, force, interactive, split) - goto tag
 *
 * POSIX: "If the tagstring is in a file with a different name than the current
 * pathname, set the current pathname to the name of that file, and replace
 * the contents of the edit buffer with the contents of that file. In this
 * case, if no '!' is appended to the command name, and the edit buffer has
 * been modified since the last complete write, it shall be an error, unless
 * the file is successfully written as specified by the autowrite option."
 *
 * Returns TRUE if it succeeds, FALSE if it fails.
 */
bool_t
exTag(tag, force, interactive, split)
char	*tag;			/* function to search for */
bool_t	force;			/* if true, force re-edit */
bool_t	interactive;		/* true if reading from tty */
bool_t	split;			/* true if want to split */
{
    static char	last_tag[32];	/* record of last tag we were given */
    bool_t	edited = FALSE;	/* TRUE if we have edited the file */
    Xviwin	*tagwindow;	/* tmp window pointer */
    TAG		*tp;
    int		l1, l2;

    /*
     * Re-use previous tag if none given; if one is given, remember it.
     */
    if (tag == NULL || tag[0] == '\0') {
	tag = last_tag;
    } else {
	(void) strncpy(last_tag, tag, sizeof(last_tag)-1);
    }

    if (tag == NULL || tag[0] == '\0') {
	if (interactive) {
	    show_error("Usage: :tag <identifier>");
	} else {
	    beep();
	}
	return(FALSE);
    }

    gotocmd(FALSE);

    tp = tagLookup(tag, &l1, &l2);
    if (tp == NULL) {
	if (interactive) {
	    show_error(hashtable==NULL	? "No tags file"
						: "Tag not found");
	}
	return(FALSE);
    }

    /*
     * Classic vi (and POSIX) autowrite modified files even if the tag is
     * in the same file. With our options to search in an existing buffer or
     * to open a new window, we only autosave if the tag file replaces the
     * current buffer.
     */

    /*
     * If we are already editing the file, just do the search.
     *
     * If "autosplit" is set, create a new buffer window and
     * edit the file in it.
     *
     * Else just edit it in the current window.
     */
    tagwindow = xvFindWindowByName(curwin, tp->t_file);
    if (tagwindow != NULL) {
	set_curwin(tagwindow);
	edited = TRUE;

    } else if (split && xvCanSplit() && exNewBuffer(tp->t_file, 0)) {
	edited = TRUE;

    } else {
	if (!force) {
	    xvAutoWrite();
	    if (is_modified(curbuf)) {
		show_error(nowrtmsg);
		return(FALSE);
	    }
	}
	if (!exists(tp->t_file)) {
	    show_error("File not found");
	    return(FALSE);
	}
	if (!exEditFile(force, tp->t_file)) {
	    return(FALSE);
	}
	edited = TRUE;
    }

    /*
     * Finally, search for the given pattern in the file,
     * but only if we successfully edited it. Note that we
     * always use curwin at this stage because it is not
     * necessarily the same as our "window" parameter.
     */
    if (edited) {
	int	old_rxtype;
	bool_t	old_wrapscan;
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
		return(FALSE);
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
	    /*
	     * If we are searching in the same file and are at a position
	     * after the target, the search would fail is wrapscan were off.
	     */
	    old_wrapscan = Pb(P_wrapscan);
	    set_param(P_wrapscan, TRUE);

	    p = xvDoSearch(pattern, '/');
	    free(pattern);
	    if (p != NULL) {
		setpcmark();
		move_cursor(p->p_line, p->p_index);
		curwin->w_set_want_col = TRUE;
		show_file_info(TRUE);
	    } else {
		beep();
	    }
	    set_param(P_regextype, old_rxtype, (char **) NULL);
	    set_param(P_wrapscan, old_wrapscan);
	    break;

	default:
	    show_error("Ill-formed tag pattern \"%s\"", tp->t_locator);
	}
	move_window_to_cursor();
	redraw_all(FALSE);
    }

    return(edited);
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
    tp = alloc(sizeof(TAG) + strlen(line) + 1);
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
    register int	length, offset;
    int			max_chars;

    if (name[0] == '\0') {
	*offsetptr = 0;
	return NULL;
    }

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
    for (offset = length; *cp != '\0' && IDCHAR(*cp); cp++) {
	offset++;
    }
    *offsetptr = offset;

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
	    free(tmp);
	}
    }
    free(hashtable);
    hashtable = NULL;
}

/*
 * If someone sets a tags-related parameter, we just invalidate the
 * entire tag cache, causing an automatic reload at next reference.
 */
/*ARGSUSED*/
bool_t
tagSetParam(new_value, interactive)
Paramval	new_value;
bool_t		interactive;
{
    tagFree();
    return(TRUE);
}
