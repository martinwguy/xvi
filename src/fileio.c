/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    fileio.c
* module function:
    File i/o routines.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

#ifdef	MEGAMAX
overlay "fileio"
#endif

/*
 * Definition of a text file format.
 *
 * This structure may need additional entries to cope with very strange file
 * formats (such as VMS).
 */
struct tfformat
{
    int		    tf_eolnchars[2];	/* end of line markers */
    int		    tf_eofchar;		/* end of file marker */
};

/*
 * Names of values for the P_format enumerated parameter.
 *
 * It is essential that these are in the same order as the fmt_...
 * symbolic constants defined in xvi.h.
 */
char	*fmt_strings[] = {
	"cstring",
	"macintosh",
	"msdos",
	"os2",
	"qnx",
	"tos",
	"unix",
	NULL,
};

/*
 * Format structures.
 *
 * It is essential that these are in the same order as the fmt_...
 * symbolic constants defined in xvi.h.
 *
 * We don't use '\r' or '\n' to define the end-of-line characters
 * because some compilers interpret them differently & this code has
 * to work the same on all systems.
 */
#define	NOCHAR	EOF

static const struct tfformat tftable [] = {
    { { '\0',	NOCHAR	}, EOF	   },	/* fmt_CSTRING */
    { { '\015', NOCHAR	}, EOF	   },	/* fmt_MACINTOSH */
    { { '\015', '\012'	}, '\032'  },	/* fmt_MSDOS */
    { { '\015', '\012'	}, '\032'  },	/* fmt_OS2 */
    { { '\036',	NOCHAR	}, EOF	   },	/* fmt_QNX */
    { { '\015', '\012'	}, EOF	   },	/* fmt_TOS */
    { { '\012', NOCHAR	}, EOF	   }	/* fmt_UNIX */
};

/*
 * Index of last entry in tftable.
 */
#define	TFMAX	(sizeof tftable / sizeof (struct tfformat) - 1)

/*
 * Current text file format.
 */
static struct tfformat curfmt;

#define eolnchars	curfmt.tf_eolnchars
#define eofchar		curfmt.tf_eofchar

/*
 * Name of current text file format.
 */
static char *fmtname = "INTERNAL ERROR";

/*
 * Copy the tftable entry indexed by tfindex into curfmt & update
 * fmtname. Return FALSE if the parameter is invalid, otherwise TRUE.
 *
 * This is called from set_format() (below).
 *
 * Note that we copy a whole tfformat structure here, instead of just copying
 * a pointer. This is so that curfmt.eolnchars & curfmt.eofchar will compile
 * to absolute address references instead of indirections, which should be
 * significantly more efficient because they are referenced for every
 * character we read or write.
 */
static bool_t
txtformset(tfindex)
int	tfindex;
{
    if (tfindex < 0 || tfindex > TFMAX)
	return FALSE;
    (void) memcpy((char *) &curfmt, (const char *) &tftable[tfindex],
						  sizeof curfmt);
    fmtname = fmt_strings[tfindex];
    return TRUE;
}

/*
 * Check value of P_format parameter.
 */
bool_t
set_format(window, new_value, interactive)
Xviwin	*window;
Paramval new_value;
bool_t	interactive;
{
    if (!txtformset(new_value.pv_i)) {
	if (interactive) {
	    show_error(window, "Invalid text file format (%d)",
	    					new_value.pv_i);
	}
	return(FALSE);
    }
    return(TRUE);
}

/*
 * Try to guess what format a text file is in, but don't waste too
 * much time on it.
 */
static int
guess_format(fp, guessp)
register FILE	*fp;
int		*guessp;
{
    for (;;) {
	switch (getc(fp)) {
	case EOF:
	    return FALSE;
	case '\0':
	    *guessp = fmt_CSTRING;
	    return TRUE;
	case '\012':
	    *guessp = fmt_UNIX;
	    return TRUE;
	case '\015':
	    *guessp = (getc(fp) == '\012' ?
#ifdef OS2
			    fmt_OS2
#else
			    fmt_MSDOS
#endif
				    : fmt_MACINTOSH);
	    return TRUE;
	case '\036':
	    *guessp = fmt_QNX;
	    return TRUE;
	}
    }
}

/*
 * Change the current file format to whatever format this file appears
 * to be in.
 */
static void
autodetect(fp)
FILE	*fp;
{
    int guess;

    if (guess_format(fp, &guess)) {
	(void) txtformset(guess);
	set_param(P_format, guess, (char **) NULL);
	P_setchanged(P_format);
    }
    (void) rewind(fp);
}

/*
 * Initial allocation for line.
 */
static Line *
initline()
{
    Line	*lp;

    /*
     * 81 bytes (80 characters + delimiter) seems like a reasonable
     * starting point; most lines in text files shouldn't be longer
     * than this. Actually it doesn't matter that much because the
     * line will eventually be adjusted by lnresize() anyway, but we
     * don't want too many calls to realloc() because get_file() needs
     * to be as fast as possible.
     *
     * If we can't get a line of 81 characters, either we're running
     * out of memory or it's got very badly fragmented, so we try for
     * a minimal size & let lnresize() adjust it later.
     */
    if ((lp = newline(81)) == NULL) {
	lp = newline(1);
    }
    return lp;
}

/*
 * Read in the given file, filling in the given "head" and "tail"
 * arguments with pointers to the first and last elements of the
 * linked list of Lines; if nothing was read, both pointers are set to
 * NULL. The return value is the number of lines read, if successful
 * (this can be 0 for an empty file), or an error return code, which
 * can be gf_NEWFILE, gf_CANTOPEN, gf_IOERR or gf_NOMEM.
 *
 * If there is an error, such as not being able to read the file or
 * running out of memory, an error message is printed; otherwise, a
 * statistics line is printed using show_message().
 *
 * The "extra_str" string is printed just after the filename in the
 * displayed line, and is typically used for "Read Only" messages. If
 * the file doesn't appear to exist, the filename is printed again,
 * immediately followed by the "no_file_str" string, & we return
 * gf_NEWFILE.
 */
long
get_file(window, filename, headp, tailp, extra_str, no_file_str)
Xviwin		*window;
char		*filename;
Line		**headp;
Line		**tailp;
char		*extra_str;
char		*no_file_str;
{
    register FILE	*fp;		/* ptr to open file */
#ifndef i386
    register
#endif
	unsigned long	nchars;		/* number of chars read */
    unsigned long	nlines;		/* number of lines read */
    unsigned long	nulls;		/* number of null chars */
    bool_t		incomplete;	/* incomplete last line */
    Line		*lptr = NULL;	/* pointer to list of lines */
    Line		*last = NULL;	/*
					 * last complete line
					 * read in
					 */
    Line		*lp;		/*
					 * line currently
					 * being read in
					 */
    register enum {
	at_soln,
	in_line,
	got_eolnc0,
	at_eoln,
	at_eof
    }			state;
    register char	*buff;		/*
					 * text of line
					 * being read in
					 */
    register int	col;		/* current column in line */
    unsigned		savecho;

    if (P_ischanged(P_format)) {
	show_message(window, "\"%s\" [%s]%s", filename, fmtname, extra_str);
    } else {
	show_message(window, "\"%s\"%s", filename, extra_str);
    }

    fp = fopenrb(filename);
    if (fp == NULL) {
	*headp = *tailp = NULL;
	if (exists(filename)) {
	    show_error(window, "Can't read \"%s\"", filename);
	    return(gf_CANTOPEN);
	} else {
	    show_message(window, "\"%s\"%s", filename, no_file_str);
	    return(gf_NEWFILE);
	}
    }

#ifdef	SETVBUF_AVAIL
    {
	unsigned int	bufsize;

	bufsize = READBUFSIZ;

	/*
	 * Keep trying to set the buffer size to something
	 * large, reducing the size by 1/2 each time.
	 * This will eventually work, and will not usually
	 * take very many calls. (jmd)
	 */
	while (setvbuf(fp, (char *) NULL, _IOFBF, bufsize) != 0 &&
						bufsize > 1) {
	    bufsize /= 2;
	}
    }
#endif /* SETVBUF_AVAIL */

    nchars = nlines = nulls = 0;
    col = 0;
    incomplete = FALSE;
    state = at_soln;
    savecho = echo;
    echo &= ~e_ALLOCFAIL;

    if (Pb(P_autodetect)) {
	autodetect(fp);
    }

    while (state != at_eof) {

	register int	c;

	c = getc(fp);

	if (c == EOF || c == eofchar) {
	    if (state != at_soln) {
		/*
		 * Reached EOF in the middle of a line; what
		 * we do here is to pretend we got a properly
		 * terminated line, and assume that a
		 * subsequent getc will still return EOF.
		 */
		incomplete = TRUE;
		state = at_eoln;
	    } else {
		state = at_eof;
		break;
	    }
	} else {
	    nchars++;

	    switch (state) {
	    case at_soln:
	    {
		/*
		 * We're at the start of a line, & we've got at least one
		 * character, so we have to allocate a new Line structure.
		 *
		 * If we can't do it, we throw away the lines we've
		 * read in so far, & return gf_NOMEM.
		 */
		if ((lp = initline()) == NULL) {
		    if (lptr != NULL) {
			throw(lptr);
		    }
		    (void) fclose(fp);
		    *headp = *tailp = NULL;
		    nlines = gf_NOMEM;
		    goto nomem;
		} else {
		    buff = lp->l_text;
		}
	    }
	    case in_line:
		if (c == eolnchars[0]) {
		    if (eolnchars[1] == NOCHAR) {
			state = at_eoln;
		    } else {
			state = got_eolnc0;
			continue;
		    }
		} else if (c == eolnchars[1]) {
		    /*
		     * If we get the second end-of-line marker, but
		     * not the first, then we're probably reading an
		     * MS-DOS file with some carriage returns missing;
		     * so just ignore the missing first character, it
		     * makes life easier.
		     */
		    state = at_eoln;
		}
		break;
	    case got_eolnc0:
		if (c == eolnchars[1]) {
		    state = at_eoln;
		} else {
		    /*
		     * We don't seem to have an end-of-line sequence
		     * after all; just take the first character
		     * literally.
		     */
		    state = in_line;
		    (void) ungetc(c, fp);
		    c = eolnchars[0];
		}
	    default:
		break;
	    }
	}

	if (state == at_eoln) {
	    /*
	     * First null-terminate the old line.
	     */
	    buff[col] = '\0';

	    /*
	     * If this fails, we squeak at the user and
	     * then throw away the lines read in so far.
	     */
	    if (!lnresize(lp, (unsigned) col + 1)) {
		if (lptr != NULL)
		    throw(lptr);
		(void) fclose(fp);
		*headp = *tailp = NULL;
		nlines = gf_NOMEM;
		goto nomem;
	    }

	    /*
	     * Tack the line onto the end of the list,
	     * and then point "last" at it.
	     */
	    if (lptr == NULL) {
		lptr = lp;
		last = lptr;
	    } else {
		last->l_next = lp;
		lp->l_prev = last;
		last = lp;
	    }

	    nlines++;
	    col = 0;
	    state = at_soln;
	} else {
	    /*
	     * Nulls are special; they can't show up in the file.
	     */
	    if (c == '\0') {
		nulls++;
		continue;
	    }
	    if (col >= lp->l_size - 1) {
		if (!lnresize(lp, col + 2)) {
		    if (lptr != NULL)
			throw(lptr);
		    (void) fclose(fp);
		    *headp = *tailp = NULL;
		    nlines = gf_NOMEM;
		    goto nomem;
		}
		buff = lp->l_text;
	    }
	    state = in_line;
	    buff[col++] = c;
	}
    }
    (void) fclose(fp);

    {
	/*
	 * Assemble error messages for status line.
	 */
	Flexbuf		errbuf;
	char		*errs;

	flexnew(&errbuf);
	if (nulls > 0) {
	    (void) lformat(&errbuf, " (%ld null character%s)",
		       nulls, (nulls == 1 ? "" : "s"));
	}
	if (incomplete) {
	    (void) lformat(&errbuf, " (incomplete last line)");
	}

	/*
	 * Show status line.
	 */
	errs = flexgetstr(&errbuf);
	if (P_ischanged(P_format)) {
	    show_message(window, "\"%s\" [%s]%s %ld/%ld%s",
				filename, fmtname, extra_str,
				nlines, nchars, errs);
	} else {
	    show_message(window, "\"%s\"%s %ld/%ld%s",
				filename, extra_str, nlines, nchars, errs);
	}
	flexdelete(&errbuf);
    }

    *headp = lptr;
    *tailp = last;

nomem:
    if (nlines == gf_NOMEM) {
	show_error(window, "Out of memory trying to read \"%s\"", filename);
    }
    echo = savecho;
    return(nlines);
}

/*
 * appendit - append to file 'fname' lines 'start' through 'end'
 *
 * If either 'start' or 'end' are NULL, the default
 * is to use the start or end of the file respectively.
 */
bool_t
appendit(window, fname, start, end, force)
Xviwin	*window;
char	*fname;
Line	*start, *end;
bool_t	force;
{
    FILE		*fp;
    unsigned long	nc;
    unsigned long	nl;

    show_message(window,
	    (P_ischanged(P_format) ? "\"%s\" [%s]" :  "\"%s\""),
						fname, fmtname);

    if (!force && !exists(fname)) {
	show_error(window, "\"%s\" No such file or directory", fname);
	return(FALSE);
    }
    if (!can_write(fname)) {
	show_error(window, "\"%s\" Permission denied", fname);
	return(FALSE);
    }

    fp = fopenab(fname);
    if (fp == NULL) {
	show_error(window, "Can't write \"%s\"", fname);
	return(FALSE);
    }

    if (put_file(window, fp, start, end, &nc, &nl) == FALSE) {
	return(FALSE);
    }

    if (P_ischanged(P_format)) {
	show_message(window, "\"%s\" [%s] %ld/%ld", fname, fmtname, nl, nc);
    } else {
	show_message(window, "\"%s\" %ld/%ld", fname, nl, nc);
    }

    return(TRUE);
}

/*
 * writeit - write to file 'fname' lines 'start' through 'end'
 *
 * If either 'start' or 'end' are NULL, the default
 * is to use the start or end of the file respectively.
 *
 * Unless the "force" argument is TRUE, we do not write
 * out buffers which have the "readonly" flag set.
 */
bool_t
writeit(window, fname, start, end, force)
Xviwin	*window;
char	*fname;
Line	*start, *end;
bool_t	force;
{
    FILE		*fp;
    unsigned long	nc;
    unsigned long	nl;
    Buffer		*buffer;
    bool_t		need_preserve = FALSE;

    buffer = window->w_buffer;

    if (buffer->b_filename != NULL &&
		    strcmp(buffer->b_filename, fname) == 0) {
	/*
	 * We are trying to write the file back to the original.
	 */
	if (is_readonly(buffer) && !force) {
	    show_error(window, "\"%s\" File is read only", fname);
	    return(FALSE);
	}
	need_preserve = TRUE;
    } else if (!force && exists(fname)) {
	show_error(window, "File exists - use \"w! %s\" to overwrite", fname);
	return(FALSE);
    }

    show_message(window,
	    (P_ischanged(P_format) ? "\"%s\" [%s]" :  "\"%s\""),
						fname, fmtname);

    /*
     * Preserve the buffer here so if the write fails it will at
     * least have been saved.
     */
    if (need_preserve && !preservebuf(window)) {
	return(FALSE);
    }

    if (!can_write(fname)) {
	show_error(window, "\"%s\" Permission denied", fname);
	return(FALSE);
    }

    fp = fopenwb(fname);
    if (fp == NULL) {
	show_error(window, "Can't write \"%s\"", fname);
	return(FALSE);
    }

    if (put_file(window, fp, start, end, &nc, &nl) == FALSE) {
	return(FALSE);
    }

    if (P_ischanged(P_format)) {
	show_message(window, "\"%s\" [%s] %ld/%ld", fname, fmtname, nl, nc);
    } else {
	show_message(window, "\"%s\" %ld/%ld", fname, nl, nc);
    }

    /*
     * Make sure any preserve file is removed if it isn't wanted.
     * It's not worth checking for the file's existence before
     * trying to remove it; the remove() will do the check anyway.
     */
    if (Pn(P_preserve) < psv_PARANOID) {
	if (buffer->b_tempfname != NULL) {
	    (void) remove(buffer->b_tempfname);
	}
    }

    /*
     * If we successfully wrote out the original file, and no start or
     * end lines were specified, or if they were specified as the start
     * and end of the buffer, then we can clear the modified status.
     */
    if (need_preserve &&
		    (start == NULL || start == buffer->b_file) &&
		    (end == NULL || end == buffer->b_lastline->l_prev)) {
	buffer->b_flags &= ~FL_MODIFIED;
    }

    return(TRUE);
}

/*
 * Write out the buffer between the given two line pointers
 * (which default to start and end of buffer) to the given file
 * pointer. The reference parameters ncp and nlp are filled in
 * with the number of characters and lines written to the file.
 * The return value is TRUE for success, FALSE for all kinds of
 * failure.
 */
bool_t
put_file(window, f, start, end, ncp, nlp)
Xviwin		*window;
register FILE	*f;
Line		*start, *end;
unsigned long	*ncp, *nlp;
{
#ifndef i386
    register
#endif
	Line			*lp;
    register unsigned long	nchars;
    unsigned long		nlines;
    Buffer			*buffer;

    buffer = window->w_buffer;

#ifdef	SETVBUF_AVAIL
    {
	unsigned int	bufsize = WRTBUFSIZ;

	/*
	 * Keep trying to set the buffer size to something
	 * large, reducing the size by 1/2 each time.
	 * This will eventually work, and will not usually
	 * take very many calls. (jmd)
	 */
	while (setvbuf(f, (char *) NULL, _IOFBF, bufsize) != 0 &&
							bufsize > 1) {
	    bufsize /= 2;
	}
    }
#endif /* SETVBUF_AVAIL */

    /*
     * If we were given a bound, start there. Otherwise just
     * start at the beginning of the file.
     */
    if (start == NULL) {
	lp = buffer->b_file;
    } else {
	lp = start;
    }

    nlines = 0;
    nchars = 0;
    for ( ; lp != buffer->b_lastline; lp = lp->l_next) {

	register char	*cp;
	register int	c;

	/*
	 * Write out the characters which comprise the line.
	 * Register declarations are used for all variables
	 * which form a part of this loop, in order to make
	 * it as fast as possible.
	 */
	for (cp = lp->l_text; (c = *cp++) != '\0';) {
	    putc(c, f);
	    nchars++;
	}

	putc(eolnchars[0], f);
	nchars++;

	if (eolnchars[1] != NOCHAR) {
	    putc(eolnchars[1], f);
	    nchars++;
	}

	if (ferror(f)) {
	    (void) fclose(f);
	    return(FALSE);
	}

	nlines++;

	/*
	 * If we were given an upper bound, and we
	 * just did that line, then bag it now.
	 */
	if (end != NULL) {
	    if (end == lp)
		break;
	}
    }

    if (fclose(f) != 0) {
	return(FALSE);
    }

    /*
     * Success!
     */
    if (ncp != NULL)
	*ncp = nchars;
    if (nlp != NULL)
	*nlp = nlines;
    return(TRUE);
}
