/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    ex_cmds2.c
* module function:
    Command functions for miscellaneous ex (colon) commands.
    See ex_cmds1.c for file- and buffer-related colon commands.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

#ifdef	MEGAMAX
overlay "ex_cmds2"
#endif

static	void	WarnUnSaved P((Xviwin *));

/*
 * Run shell command.
 *
 * You might think it would be easier to do this as
 *
 *	sys_endv();
 *	system(command);
 *	sys_startv();
 *	prompt("[Hit return to continue] ");
 *	...
 *
 * but the trouble is that, with some systems, sys_startv() can be
 * used to swap display pages, which would mean that the user would
 * never have time to see the output from the command. (This applies
 * to some terminals, as well as machines with memory-mappped video;
 * cmdtool windows on Sun workstations also do the same thing.)
 *
 * This means we have to display the prompt before calling
 * sys_startv(), so we just use good old fputs(). (We're trying not to
 * use printf()).
 */
/*ARGSUSED*/
bool_t
exShellCommand(window, command)
Xviwin	*window;
char	*command;
{
    int	c;
    int	status;

    if (!subshells) {
	show_error(window, "Can't shell escape from a window");
	return(FALSE);
    }

    sys_endv();

    xvAutoWriteAll(window);
    /*
     * POSIX: "If autowrite is set, and the edit buffer has been modified
     * since it was last completely written to any file [and] the write
     * fails, it shall be an error and the command shall not be executed.
     */
    if (Pb(P_autowrite) && xvChangesNotSaved(window)) {
	show_error(window, nowrtmsg);
	return(FALSE);
    }
    WarnUnSaved(window);
    (void) fputs(command, stdout);
    (void) fputs("\r\n", stdout);
    (void) fflush(stdout);
    status = call_system(command);
    (void) fputs("[Hit return to continue] ", stdout);
    (void) fflush(stdout);
    while ((c = getc(stdin)) != '\n' && c != '\r' && c != EOF)
	;

    sys_startv();
    redraw_all(window, TRUE);

    return(status == 0);
}

void
exInteractiveShell(window, force)
Xviwin	*window;
bool_t	force;
{
    char	*sh = NULL;
    int	sysret;

    if (!subshells) {
	show_error(window, "Can't shell escape from a window");
	return;
    }

    sh = Ps(P_shell);
    if (sh == NULL) {
	show_error(window, "SHELL variable not set");
	return;
    }

    if (!force) {
	xvAutoWriteAll(window);
	if (Pb(P_autowrite) && xvChangesNotSaved(window)) {
	    show_error(window, nowrtmsg);
	    return;
	}
    }

    sys_endv();

    WarnUnSaved(window);

    sysret = call_shell(sh);

    sys_startv();
    redraw_all(window, TRUE);

    if (sysret != 0) {
#ifdef STRERROR_AVAIL
	show_error(window, "Can't execute %s [%s]", sh,
			(errno > 0 ? strerror(errno) : "Unknown Error"));
#else	/* strerror() not available */
	show_error(window, "Can't execute %s", sh);
#endif	/* strerror() not available */
    }
}

/*ARGSUSED*/
void
exSuspend(window, force)
Xviwin	*window;
bool_t	force;
{
    if (State == NORMAL) {
#ifdef	SIGSTOP
#   ifndef	POSIX
	extern	int	kill P((int, int));
	extern	int	getpid P((void));
#   endif	/* not POSIX */
	extern volatile bool_t win_size_changed;

	if (!force) {
	    xvAutoWriteAll(window);
	    if (Pb(P_autowrite) && xvChangesNotSaved(window)) {
		show_error(window, nowrtmsg);
		return;
	    }
	}

	sys_endv();

	WarnUnSaved(window);

	(void) kill(getpid(), SIGSTOP);

	/*
	 * An X window size change that happened while we were suspended
	 * does not result in a SIGWINCH signal being delivered to us
	 * so when we restart, go check the window/tty size.
	 * If it's the same as before, nothing will be done.
	 */
	win_size_changed = TRUE;

	sys_startv();
	redraw_all(window, TRUE);

#else /* SIGSTOP */

	/*
	 * Can't suspend unless we're on a BSD UNIX system;
	 * just pretend by invoking a shell instead.
	 */
	exInteractiveShell(window, force);

#endif /* SIGSTOP */
    }
}

void
exEquals(window, line)
Xviwin	*window;
Line	*line;
{
    if (line == NULL) {
	/*
	 * Default position is ".".
	 */
	line = window->w_cursor->p_line;
    }

    show_message(window, "%ld", lineno(line));
}

void
exHelp(window)
Xviwin	*window;
{
    unsigned	savecho;

    savecho = echo;
    echo &= ~(e_SCROLL | e_REPORT | e_SHOWINFO);
    if (Ps(P_helpfile) != NULL &&
		    exNewBuffer(window, fexpand(Ps(P_helpfile), FALSE), 0)) {
	/*
	 * We use "curbuf" here because the new buffer will
	 * have been made the current one by do_buffer().
	 */
	curbuf->b_flags |= FL_READONLY | FL_NOEDIT;
	move_window_to_cursor(curwin);
	show_file_info(curwin);
	redraw_window(curwin, FALSE);
    }
    echo = savecho;
}

bool_t
exSource(interactive, file)
bool_t	interactive;
char	*file;
{
    Flexbuf		cmd;
    FILE		*fp;
    register int	c;
    bool_t		literal;
    bool_t		success = TRUE;

    fp = fopen(file, "r");
    if (fp == NULL) {
	if (interactive) {
	    show_error(curwin, "Can't open \"%s\"", file);
	}
	return(FALSE);
    }

    flexnew(&cmd);
    literal = FALSE;
    while ((c = getc(fp)) != EOF) {
	if (kbdintr) {
	    kbdintr = FALSE;
	    imessage = TRUE;
	    success = FALSE;
	    break;
	}
	if (!literal && (c == CTRL('V') || c == '\n')) {
	    switch (c) {
	    case CTRL('V'):
		literal = TRUE;
		break;

	    case '\n':
		if (!flexempty(&cmd)) {
		    if (!exCommand(flexgetstr(&cmd), interactive)) {
			success = FALSE;
			goto out;	/* Alas, poor "break"! */
		    }
		    flexclear(&cmd);
		}
		break;
	    }
	} else {
	    literal = FALSE;
	    if (!flexaddch(&cmd, c)) {
		success = FALSE;
		goto out;
	    }
	}
    }
out:
    flexdelete(&cmd);
    (void) fclose(fp);
    return(success);
}

/*
 * Change directory.
 *
 * With a NULL argument, change to the directory given by the HOME
 * environment variable if it is defined; with an argument of "-",
 * change back to the previous directory (like ksh).
 *
 * Return NULL pointer if OK, otherwise error message to say
 * what went wrong.
 */
char *
exChangeDirectory(dir)
    char	*dir;
{
    static char	*homedir = NULL;
    static char	*prevdir = NULL;
    char	*ret;
    char	*curdir;

    if (dir == NULL && homedir == NULL) {
	if ((ret = getenv("HOME")) == NULL) {
	    return("HOME environment variable not set");
	}
	homedir = strsave(ret);
	if (homedir == NULL) {
	    return("Failed to save HOME environment variable");
	}
    }

    if (dir == NULL) {
	dir = homedir;
    } else if (*dir == '-' && dir[1] == '\0') {
	if (prevdir == NULL) {
	    return("No previous directory");
	} else {
	    dir = prevdir;
	}
    }

    curdir = alloc(MAXPATHLEN + 1);
    if (curdir == NULL) {
	return(out_of_memory);
    }
    if (getcwd(curdir, MAXPATHLEN + 1) == NULL) {
	free(curdir);
	curdir = NULL;
    }
    if (chdir(dir) != 0) {
	return(strerror(errno));
    }
    if (prevdir) {
	free(prevdir);
	prevdir = NULL;
    }
    if (curdir) {
	prevdir = re_alloc(curdir, strlen(curdir) + 1);
    }
    return(NULL);
}

/*
 * Perform a line-based operation, one of [dmty].
 */
bool_t
exLineOperation(type, l1, l2, destline)
int	type;			/* one of [cdmy] */
Line	*l1, *l2;		/* start and end (inclusive) of range */
Line	*destline;		/* destination line for copy/move */
{
    Posn	p1, p2;
    Posn	destpos;

    p1.p_line = (l1 != NULL) ? l1 : curwin->w_cursor->p_line;
    p2.p_line = (l2 != NULL) ? l2 : p1.p_line;
    p1.p_index = p2.p_index = 0;

    if (type == 't' || type == 'm') {
	if (destline == NULL) {
	    show_error(curwin, "No destination specified");
	    return(FALSE);
	}
    }

    /*
     * Check that the destination is not inside
     * the source range for "move" operations.
     */
    if (type == 'm') {
	unsigned long	destlineno;

	destlineno = lineno(destline);
	if (destlineno >= lineno(p1.p_line) &&
			    destlineno <= lineno(p2.p_line)) {
	    show_error(curwin, "Source conflicts with destination of move");
	    return(FALSE);
	}
    }

    /*
     * Yank the text.
     * For delete and yank, the removed text goes into the default buffer.
     * For move and copy, we use the secret buffer '=' as a temporary.
     */
    if (!do_yank(curbuf, &p1, &p2, FALSE,
		 (type == 'd' || type == 'y') ? '@' : '=')) {
	return(FALSE);
    }

    if (!start_command(curwin)) {
	return(FALSE);
    }

    switch (type) {
    case 'd':			/* delete */
    case 'm':			/* move */
	move_cursor(curwin, p1.p_line, 0);
	repllines(curwin, p1.p_line, cntllines(p1.p_line, p2.p_line),
						(Line *) NULL);
	xvUpdateAllBufferWindows(curbuf);
	cursupdate(curwin);
	begin_line(curwin, TRUE);
    }

    switch (type) {
    case 't':			/* copy */
    case 'm':			/* move */
	/*
	 * And put it back at the destination point.
	 */
	destpos.p_line = destline;
	destpos.p_index = 0;
	do_put(curwin, &destpos, FORWARD, '=');

	xvUpdateAllBufferWindows(curbuf);
	cursupdate(curwin);
    }

    end_command(curwin);

    return(TRUE);
}

/*
 * Apply the "join" operation to all lines between l1 and l2
 * inclusive, or (by default) to the current line in window.
 */
bool_t
exJoin(window, l1, l2, exclam)
Xviwin	*window;
Line	*l1, *l2;
bool_t	exclam;
{
    Line	*l3;		/* hack hack hack */
    bool_t	success;

    if (l1 == NULL) {
	l1 = curwin->w_cursor->p_line;
    }
    if (l2 == NULL) {
	l2 = l1;
    }
    l2 = l2->l_next;
    l3 = is_lastline(l2) ? l2 : l2->l_next;

    if (is_lastline(l1)) {
	beep(window);
	return(FALSE);
    }

    if (!start_command(window)) {
	return(FALSE);
    }

    success = TRUE;
    for ( ; l1 != l2 && l1 != l3; l1 = l1->l_next) {
	if (is_lastline(l1->l_next)) {
	    break;
	}
	if (!xvJoinLine(window, l1, exclam)) {
	    beep(window);
	    success = FALSE;
	    break;
	}
	/* Leave the cursor at the start of the joined line.
	 * Original "vi" either does this or leaves it where it was,
	 * apparently at random. */
        move_cursor(window, l1, 0);
    }

    xvUpdateAllBufferWindows(window->w_buffer);

    end_command(window);

    return(success);
}

static void
WarnUnSaved(window)
Xviwin	*window;
{
    if (Pb(P_warn) && xvChangesNotSaved(window)) {
	fputs("[No write since last change]\r\n", stdout);
	fflush(stdout);
    }
}
