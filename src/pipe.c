/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    pipe.c
* module function:
    Handle pipe operators.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

static	int	p_write P((FILE *));
static	long	p_read P((FILE *));

static	Xviwin	*specwin;		/* used to communicate information */
static	Line	*line1, *line2;		/*  about range of pipe command to */
static	Line	*newlines;		/*  do_pipe()                      */

static	char	*lastcmd = NULL;	/* last command (used for !!) */

/*
 * This function is called when the ! is typed in initially,
 * to specify the range; do_pipe() is then called later on
 * when the line has been typed in completely.
 *
 * Note that we record the first and last+1th lines to make the loop easier.
 */
void
specify_pipe_range(window, l1, l2)
Xviwin	*window;
Line	*l1;
Line	*l2;
{
    /*
     * Ensure that the lines specified are in the right order.
     */
    if (l1 != NULL && l2 != NULL && earlier(l2, l1)) {
	register Line	*tmp;

	tmp = l1;
	l1 = l2;
	l2 = tmp;
    }
    line1 = (l1 != NULL) ? l1 : window->w_buffer->b_file;
    line2 = (l2 != NULL) ? l2->l_next : window->w_buffer->b_lastline;
    specwin = window;
}

/*
 * Pipe the given sequence of lines through the command,
 * replacing the old set with its output.
 */
void
do_pipe(window, command)
Xviwin	*window;
char	*command;
{
    if (line1 == NULL || line2 == NULL || specwin != window) {
	show_error(window,
	    "Internal error: pipe through badly-specified range.");
	return;
    }
    if (command[0] == '!' && command[1] == '\0' && lastcmd != NULL) {
	command = lastcmd;
	show_message(window, "!%s", command);
    } else {
	if (lastcmd != NULL) {
	    free(lastcmd);
	}
	lastcmd = strsave(command);
    }

    newlines = NULL;
    if (sys_pipe(command, p_write, p_read)) {
    	if (newlines != NULL) {
	    repllines(window, line1, cntllines(line1, line2) - 1, newlines);
	    xvUpdateAllBufferWindows(window->w_buffer);
	    begin_line(window, TRUE);
	} else {
	    /*
	     * Command succeeded, but produced no output.
	     */
	    show_message(window, "Command produced no output");
	    redraw_all(window, TRUE);
	}
    } else {
	show_error(window, "Failed to execute \"%s\"", command);
	redraw_all(window, TRUE);
    }
    cursupdate(window);
}

static int
p_write(fp)
FILE	*fp;
{
    Line	*l;
    long	n;

    for (l = line1, n = 0; l != line2; l = l->l_next, n++) {
	(void) fputs(l->l_text, fp);
	(void) putc('\n', fp);
    }
    return(n);
}

/*
 * Returns the number of lines read, or -1 for failure.
 */
static long
p_read(fp)
FILE	*fp;
{
    Line	*lptr = NULL;	/* pointer to list of lines */
    Line	*last = NULL;	/* last complete line read in */
    Line	*lp;		/* line currently being read in */
    register enum {
	at_soln,
	in_line,
	at_eoln,
	at_eof
    }	state;
    char	*buff;		/* text of line being read in */
    int		col;		/* current column in line */
    unsigned long
		nlines;		/* number of lines read */

    col = 0;
    nlines = 0;
    state = at_soln;
    while (state != at_eof) {

	register int	c;

	c = getc(fp);

	/*
	 * Nulls are special; they can't show up in the file.
	 */
	if (c == '\0') {
	    continue;
	}

	if (c == EOF) {
	    if (state != at_soln) {
		/*
		 * Reached EOF in the middle of a line; what
		 * we do here is to pretend we got a properly
		 * terminated line, and assume that a
		 * subsequent getc will still return EOF.
		 */
		state = at_eoln;
	    } else {
		state = at_eof;
	    }
	} else {
	    if (state == at_soln) {
		/*
		 * We're at the start of a line, &
		 * we've got at least one character,
		 * so we have to allocate a new Line
		 * structure.
		 *
		 * If we can't do it, we throw away
		 * the lines we've read in so far, &
		 * return gf_NOMEM.
		 */
		lp = newline(1);
		if (lp == NULL) {
		    if (lptr != NULL)
			throw(lptr);
		    return(-1);
		} else {
		    buff = lp->l_text;
		}
	    }

	    if (c == '\n') {
		state = at_eoln;
	    }
	}

	switch (state) {
	case at_eof:
	    break;

	case at_soln:
	case in_line:
	    if (col >= lp->l_size - 1) {
		if (!lnresize(lp, col + 2)) {
		    if (lptr != NULL)
			throw(lptr);
		    return(-1);
		}
		buff = lp->l_text;
	    }

	    state = in_line;
	    buff[col++] = c;
	    break;

	case at_eoln:
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
		return(-1);
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
	    break;
	}
    }

    newlines = lptr;

    return(nlines);
}

/*
 * Write to a shell command.
 *
 * Writing to the input of a command means it may well be interactive,
 * so we treat it in the same way as invoking a normal subshell. See
 * comments for exShellCommand() in ex_cmds2.c.
 */
void
xvWriteToCommand(window, command, l1, l2)
Xviwin	*window;
char	*command;
Line	*l1, *l2;
{
    int		c;
    bool_t	success;

    if (!subshells) {
    	show_error(window, "Can't shell escape from a window");
	return;
    }

    if (l1 == NULL) {
	line1 = window->w_buffer->b_file;
    } else {
	line1 = l1;
    }
    if (l2 == NULL) {
	line2 = window->w_buffer->b_lastline;
    } else {
	line2 = l2->l_next;
    }
    if (command[0] == '!' && command[1] == '\0' && lastcmd != NULL) {
	command = lastcmd;
    } else {
	if (lastcmd != NULL) {
	    free(lastcmd);
	}
	lastcmd = strsave(command);
    }

    sys_endv();

    (void) fputs(command, stdout);
    (void) fputs("\r\n", stdout);
    (void) fflush(stdout);

    success = sys_pipe(command, p_write, NOFUNC);

    (void) fputs("[Hit return to continue] ", stdout);
    (void) fflush(stdout);
    while ((c = getc(stdin)) != '\n' && c != '\r' && c != EOF)
	;

    sys_startv();

    if (!success) {
	show_error(window, "Failed to execute \"%s\"", command);
    }

    redraw_all(window, TRUE);
    cursupdate(window);
}

/*
 * Read input from a shell command.
 *
 * Note that we do not echo the command onto stdout here - because
 * doing so means we have to do sys_endv(), which we can only use
 * if subshells is TRUE. This is restrictive, so we don't bother.
 *
 * All this assumes that the command is non-interactive, and if this
 * assumption is misplaced we may be in some serious shit. Ho hum.
 */
void
xvReadFromCommand(window, command, atline)
Xviwin	*window;
char	*command;
Line	*atline;
{
    int		c;
    bool_t	success;

    if (command[0] == '!' && command[1] == '\0' && lastcmd != NULL) {
	command = lastcmd;
    } else {
	if (lastcmd != NULL) {
	    free(lastcmd);
	}
	lastcmd = strsave(command);
    }

    newlines = NULL;
    success = sys_pipe(command, NOFUNC, p_read);

    if (success) {
    	if (newlines != NULL) {
	    repllines(window, atline->l_next, 0L, newlines);
	    xvUpdateAllBufferWindows(window->w_buffer);
	    begin_line(window, TRUE);
	} else {
	    /*
	     * Command succeeded, but produced no output.
	     */
	    show_message(window, "Command produced no output");
	}
    } else {
	show_error(window, "Failed to execute \"%s\"", command);
    }
    redraw_all(window, TRUE);
    cursupdate(window);
}
