/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    ex_cmds1.c
* module function:
    File, window and buffer-related command functions
    for ex (colon) commands.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

#ifdef	MEGAMAX
overlay "ex_cmds1"
#endif

static	char	**files;	/* list of input files */
static	int	numfiles;	/* number of input files */
static	int	curfile;	/* number of the current file */

static	char	nowrtmsg[] = "No write since last change (use ! to override)";
static	char	nowrtbufs[] = "Some buffers not written (use ! to override)";

static	bool_t	more_files P((void));

void
exQuit(window, force)
Xviwin	*window;
bool_t	force;
{
    Xviwin	*wp;
    bool_t	canexit;

    if (force) {
	canexit = TRUE;
    } else {
	if (xvChangesNotSaved(window)) {
	    show_error(window, nowrtbufs);
	    canexit = FALSE;
	} else {
	    canexit = ! more_files();
	}
    }

    if (canexit) {
	/*
	 * Remove any preserve files we may have written - we don't
	 * want to just leave them lying around, it's messy.
	 */
	wp = window;
	do {
	    unpreserve(window->w_buffer);
	} while ((wp = xvNextWindow(wp)) != window);

	State = EXITING;
    }
}

/*
 * Split the current window into two, leaving both windows mapped
 * onto the same buffer.
 */
void
exSplitWindow(window)
Xviwin	*window;
{
    Xviwin		*newwin;
    Posn		*curposn;
    unsigned		savecho;

    newwin = xvOpenWindow(window, 0);
    if (newwin == NULL) {
	return;
    }

    xvMapWindowOntoBuffer(newwin, window->w_buffer);

    /*
     * Update the status line of the old window
     * (since it will have been moved).
     * Also update the window - this will almost certainly
     * have no effect on the screen, but is necessary.
     */
    show_file_info(window, TRUE);
    redraw_window(window, FALSE);

    /*
     * Show the new window.
     *
     * At this stage, we only call redraw_window() because we want
     * newwin->w_botline to be updated; we don't let it do any actual
     * screen updating until after we've called
     * move_window_to_cursor().
     */
    savecho = echo;
    echo &= ~(e_CHARUPDATE | e_SCROLL | e_REPORT | e_SHOWINFO);

    init_sline(newwin);
    curposn = window->w_cursor;
    move_cursor(newwin, curposn->p_line, curposn->p_index);
    redraw_window(newwin, FALSE);

    echo = savecho;

    move_window_to_cursor(newwin);
    cursupdate(newwin);

    redraw_window(newwin, FALSE);
    show_file_info(newwin, TRUE);

    /*
     * Update the global window variable.
     */
    curwin = newwin;
}

/*
 * Open a new buffer window, with a possible filename arg.
 *
 * exNewBuffer() is responsible for updating the screen image for the
 * old window, but not the new one, since we may want to move to a
 * different location in the new buffer (e.g. for a tag search).
 *
 * The "sizehint" argument is the number of lines we would like the
 * old buffer window to occupy after the split. If it's 0, we just
 * split the window evenly.
 */
bool_t
exNewBuffer(window, filename, sizehint)
Xviwin	*window;
char	*filename;
int	sizehint;
{
    Buffer	*new;
    Xviwin	*newwin;

    new = new_buffer();
    if (new == NULL) {
	show_error(window, "No more buffers!");
	return(FALSE);
    }
    newwin = xvOpenWindow(window, sizehint);
    if (newwin == NULL) {
	free_buffer(new);
	return(FALSE);
    }

    xvMapWindowOntoBuffer(newwin, new);

    /*
     * Update the status lines of each buffer.
     *
     * Even if (echo & e_SHOWINFO) is turned off, show_file_info()
     * will always call update_sline(), which is what we really
     * need here.
     *
     * Note that we don't need to call move_window_to_cursor() for
     * the old window until it becomes the current window again.
     */
    show_file_info(window, TRUE);
    init_sline(newwin);

    if (filename != NULL) {
	(void) exEditFile(newwin, FALSE, filename);
    } else {
	new->b_filename = new->b_tempfname = NULL;
	show_file_info(newwin, TRUE);
    }

    redraw_window(window, FALSE);

    /*
     * The current buffer (a global variable) has
     * to be updated here. No way around this.
     */
    curbuf = new;
    curwin = newwin;

    return(TRUE);
}

/*
 * "close" (the current window).
 */
void
exCloseWindow(win, force)
Xviwin	*win;
bool_t	force;
{
    Buffer	*buffer;

    buffer = win->w_buffer;

    /*
     * If the window is the only one onto a modified buffer, and
     * they are not forcing the close, don't allow it to happen.
     */
    if (is_modified(buffer) && !force && buffer->b_nwindows < 2) {
	show_error(win, nowrtmsg);
	return;
    }

    if (win == xvNextWindow(win)) {
    	if (more_files()) {
	    /*
	     * If there are more files to be edited, and no other
	     * windows are open, we should disallow the close.
	     */
	    return;
	} else {
	    (void) xvCloseWindow(win);
	    State = EXITING;
	    return;
	}
    }

    if (buffer->b_nwindows == 1 && buffer->b_filename != NULL) {
	/*
	 * Before we free the buffer, save its filename.
	 */
	push_alternate(buffer->b_filename,
			lineno(buffer, win->w_cursor->p_line));
	free(buffer->b_filename);
	buffer->b_filename = NULL;

	/*
	 * Also remove any preserve file we may have created,
	 * and free up b_tempfname.
	 */
	unpreserve(buffer);
    }

    /*
     * Close the old window, and move to the next one.
     * Have to update the globals "curbuf" and "curwin" here.
     */
    curwin = xvCloseWindow(win);
    if (curwin == NULL) {
    	/*
	 * This is not supposed to happen; if this is the last window
	 * open, we should have spotted it above. Still, cope with it...
	 */
	State = EXITING;
	return;
    }

    curbuf = curwin->w_buffer;
    if (buffer->b_nwindows == 0) {
	free_buffer(buffer);
    }

    {
	unsigned	savecho;

	savecho = echo;
	/*
	 * Adjust position of new current window
	 * within buffer before updating it, to avoid
	 * wasting screen output - but don't do any
	 * scrolling at this stage because the old
	 * window is still on the screen.
	 */
	echo &= ~(e_CHARUPDATE | e_SHOWINFO | e_SCROLL);
	move_window_to_cursor(curwin);
	echo = savecho;

    }
    redraw_window(curwin, FALSE);
    show_file_info(curwin, TRUE);
}

/*
 * Close current window.
 *
 * If it is the last window onto the buffer, also close the buffer.
 *
 * If the buffer has been modified, we must write it out before closing it.
 */
void
exXit(window)
Xviwin	*window;
{
    Buffer	*buffer;

    buffer = window->w_buffer;

    if (is_modified(buffer) && buffer->b_nwindows < 2) {
	if (buffer->b_filename != NULL) {
	    if (!writeit(window, buffer->b_filename,
				(Line *) NULL, (Line *) NULL, FALSE)) {
		return;
	    }
	} else {
	    show_error(window, "No output file");
	    return;
	}
    }

    exCloseWindow(window, FALSE);
}

/*
 * Edit the given filename in the given buffer,
 * replacing any current contents.  Note that the
 * screen is not updated, since there are routines
 * which use this function before moving the cursor
 * to a different position in the file.
 *
 * Returns TRUE for success, FALSE for failure.
 */
bool_t
exEditFile(window, force, arg)
Xviwin	*window;
bool_t	force;
char	*arg;
{
    long	line = 1;		/* line # to go to in new file */
    long	nlines;			/* no of lines read from file */
    Line	*head;			/* start of list of lines */
    Line	*tail;			/* last element of list of lines */
    bool_t	readonly;		/* true if cannot write file */
    Buffer	*buffer;
    Xviwin	*wp;

    buffer = window->w_buffer;

    if (!force && is_modified(buffer)) {
	show_error(window, nowrtmsg);
	return(FALSE);
    }

    if (arg == NULL || arg[0] == '\0') {
	/*
	 * No filename specified; we must already have one.
	 */
	if (buffer->b_filename == NULL) {
	    show_error(window, "No filename");
	    return(FALSE);
	}
    } else /* arg != NULL */ {
	/*
	 * Filename specified.
	 */

	/*
	 * Detect a ":e" on the current file. This is mainly for ":ta"
	 * commands where the destination is within the current file.
	 * If we are editing the same file, and it has not changed,
	 * don't bother reading it in.
	 */
	if (buffer->b_filename != NULL && strcmp(arg, buffer->b_filename) == 0) {
	    if (!is_modified(buffer) || (is_modified(buffer) && !force)) {
		return(TRUE);
	    }
	} else {

	    /*
	     * We are editing a different file.
	     */

	    /*
	     * Detect an edit of the alternate file, and pop the alternate
	     * stack, setting the line number in the process.
	     */
	    if (alt_file_name() != NULL && strcmp(arg, alt_file_name()) == 0) {
		(void) pop_alternate(&line);
	    }

	    /*
	     * Push the old filename onto the alternate stack, and then free it.
	     */
	    if (buffer->b_filename != NULL) {
		push_alternate(buffer->b_filename,
				    lineno(buffer, window->w_cursor->p_line));
		free(buffer->b_filename);
		buffer->b_filename = NULL;
	    }

	    /*
	     * Install the new one.
	     */
	    buffer->b_filename = strsave(arg);
	    if (buffer->b_filename == NULL) {
		return(FALSE);
	    }
	}

	/*
	 * We are no longer editing the same file, so there is no point in
	 * keeping a preserve file relating to it.
	 */
	unpreserve(buffer);
    }

    /*
     * Clear out the old buffer and read the file.
     */
    if (clear_buffer(buffer) == FALSE) {
	show_error(window, "Out of memory");
	return(FALSE);
    }

    /*
     * Be sure to re-map all window structures onto the buffer,
     * in order to eliminate any pointers into the old buffer.
     */
    wp = window;
    do {
	if (wp->w_buffer != buffer)
	    continue;

	xvUnMapWindow(wp);
	xvMapWindowOntoBuffer(wp, buffer);

    } while ((wp = xvNextWindow(wp)) != window);

    readonly = Pb(P_readonly) || !can_write(buffer->b_filename);

    nlines = get_file(window, buffer->b_filename, &head, &tail,
				    (readonly ? " [Read only]" : ""),
				    " [New file]");

    update_sline(window);		/* ensure colour is updated */

    wp = window;
    while ((wp = xvNextDisplayedWindow(wp)) != window) {
	/*
	 * If there are any other windows on to the same buffer, make
	 * sure they're properly updated.
	 */
	if (wp->w_buffer == buffer) {
	    show_message(wp, "%s", sline_text(window));
	}
    }

    if (nlines == gf_NEWFILE) {	/* no such file */
	return(FALSE);
    } else if (nlines >= 0) {
	unsigned savecho;

	/*
	 * Success.
	 */
	if (readonly) {
	    buffer->b_flags |= FL_READONLY;
	    if (Pb(P_autonoedit)) {
		buffer->b_flags |= FL_NOEDIT;
	    }
	} else {
	    buffer->b_flags &= ~(FL_READONLY | FL_NOEDIT);
	}

	if (nlines == 0) {	/* empty file */
	    return(TRUE);
	}

	/*
	 * We have successfully read the file in,
	 * so now we must link it into the buffer.
	 */
	replbuffer(window, head);

	move_cursor(window, gotoline(buffer, (unsigned long) line), 0);
	begin_line(window, TRUE);
	setpcmark(window);

	/*
	 * We only call redraw_window() here because we want
	 * window->w_botline to be updated; we don't let it do any
	 * actual screen updating, for the reason explained above.
	 */
	savecho = echo;
	echo &= ~(e_CHARUPDATE | e_SCROLL | e_REPORT | e_SHOWINFO);
	redraw_window(window, FALSE);
	echo = savecho;

	return(TRUE);
    } else {
	/*
	 * We failed to read in the file. An appropriate
	 * message will already have been printed by
	 * get_file() (or alloc()). Don't forget to save
	 * the filename as the new alternate filename.
	 */

	push_alternate(buffer->b_filename, 1);
	if (buffer->b_filename != NULL)
	    free(buffer->b_filename);
	if (buffer->b_tempfname != NULL)
	    free(buffer->b_tempfname);
	buffer->b_filename = buffer->b_tempfname = NULL;
	return(FALSE);
    }
}

static int curr_arg;

static char *
show_arg()
{
    static Flexbuf	fb;
    int			c;

    if (curr_arg >= numfiles) {
	return (char *) NULL;
    }
    flexclear(&fb);
    (void) lformat(&fb, "%c%s%c",
		 (curr_arg == curfile ? '[' : ' '),
		 files[curr_arg],
		 (curr_arg == curfile ? ']' : ' '));
    curr_arg++;
    return flexgetstr(&fb);
}

void
exArgs(window)
Xviwin	*window;
{
    Flexbuf	fb;
    int		colwidth;
    int		count;
    int		maxcols;

    if (numfiles == 0) {
	show_message(window, "No files");
	return;
    }

    colwidth = 0;
    maxcols = window->w_ncols - window->w_spare_cols;
    flexnew(&fb);
    for (count = 0; count < numfiles; count++) {
	char	*fn;
	int	width;

	if ((fn = files[count]) != NULL) {
	    if ((width = strlen(fn) + 3) > colwidth) {
		colwidth = width;
	    }
	    if (flexlen(&fb) <= maxcols) {
		if (count > 0) {
		    (void) flexaddch(&fb, ' ');
		}
		if (count == curfile) {
		    (void) flexaddch(&fb, '[');
		}
		(void) lformat(&fb, "%s", fn);
		if (count == curfile) {
		    (void) flexaddch(&fb, ']');
		}
	    }
	}
    }
    if (flexlen(&fb) <= maxcols) {
	show_message(window, "%s", flexgetstr(&fb));
    } else {
	curr_arg = 0;
	disp_init(window, show_arg, colwidth, FALSE);
    }
    flexdelete(&fb);
}

/*
 * Change the current file list to the one specified, or edit the next
 * file in the current file list, or edit the next file in the list if
 * no argument is given.
 */
void
exNext(window, argc, argv, force)
Xviwin	*window;
int	argc;
char	*argv[];
bool_t	force;
{
    unsigned	savecho;

    savecho = echo;
    if (argc > 0) {
	int	count;
	int	winsize;
	int	sparelines;
	int	newnw;

	/*
	 * Arguments given - this means a new set of filenames.
	 */
	if (!force && is_modified(window->w_buffer)) {
	    show_error(window, nowrtmsg);
	    return;
	}

	/*
	 * There were no files before, so start from square one.
	 */
	if (numfiles == 0) {
	    files = (char **) alloc((unsigned) argc * sizeof(char *));
	    if (files == NULL) {
		return;
	    }
	} else {
	    /*
	     * We can change the existing list of files.
	     * Free up all the individual filenames
	     * which we got last time.
	     */
	    for (count = 0; count < numfiles; count++) {
		free(files[count]);
	    }
	    if (argc != numfiles) {
		files = (char **) realloc((char *) files,
				    (unsigned) argc * sizeof(char *));
		if (files == NULL) {
		    numfiles = 0;
		    return;
		}
	    }
	}

	/*
	 * Now record all the new filenames.
	 */
	for (count = 0; count < argc; count++) {
	    files[count] = strsave(argv[count]);
	    if (files[count] == NULL) {
		/*
		 * Aargh. Failed half-way through.
		 * Clean up the mess ...
		 */
		while (--count >= 0)
		    free(files[count]);
		free((char *) files);
		files = NULL;
		numfiles = 0;
		return;
	    }
	}
	numfiles = argc;
	curfile = 0;

	/*
	 * And try to edit the first few of them.
	 *
	 * In this case, we don't want report() or
	 * show_file_info() to be called, because otherwise
	 * the messages printed by get_file() won't be seen.
	 */
	echo &= ~(e_SCROLL | e_REPORT | e_SHOWINFO);

	(void) exEditFile(curwin, force, files[0]);

	/*
	 * Update the current window before
	 * creating any new ones.
	 */
	move_window_to_cursor(curwin);

	/*
	 * If equalise is set, we have to work out how many files we
	 * can edit, and the size of the new windows to be created.
	 */
	if (Pb(P_equalsize)) {
	    Xviwin	*wp;
	    int		oldnw;
	    int		totalrows;

	    wp = curwin;
	    oldnw = 0;
	    do {
		wp = xvNextWindow(wp);
		oldnw++;
	    } while (wp != window);

	    newnw = numfiles - curfile;
	    totalrows = VSrows(curwin->w_vs);
	    if ((totalrows / MINROWS) < newnw) {
		newnw = (totalrows / MINROWS);
	    }
	    if (newnw > Pn(P_autosplit)) {
		newnw = Pn(P_autosplit);
	    }
	    if (newnw > oldnw) {
		winsize = (totalrows / newnw);
		sparelines = totalrows - (newnw * winsize);
	    }
	    xvEqualiseWindows(newnw);
	}

	while ((curfile + 1) < numfiles && xvCanSplit(curwin)) {
	    bool_t	success;

	    success = exNewBuffer(curwin, files[++curfile], 
			    Pb(P_equalsize) ? (winsize + sparelines) : 0);
	    if (Pb(P_equalsize) && sparelines > 0) {
		sparelines = 0;
	    }

	    /*
	     * Make sure move_window_to_cursor() is called
	     * for every window before calling
	     * xvUpdateAllBufferWindows().
	     */
	    move_window_to_cursor(curwin);
	    if (!success) {
		--curfile;
		break;
	    }
	    xvEqualiseWindows(newnw);
	}
	xvEqualiseWindows(0);
	redraw_window(curwin, FALSE);

    } else if ((curfile + 1) < numfiles) {
	/*
	 * No arguments; this is the normal usage, and
	 * indicates we should edit the next file in the list.
	 * Don't grab the next file if the current one is
	 * modified and not written, or we will "lose"
	 * files from the list.
	 */
	if (!force && is_modified(window->w_buffer)) {
	    show_error(window, nowrtmsg);
	    return;
	}

	/*
	 * Just edit the next file.
	 */
	echo &= ~(e_SCROLL | e_REPORT | e_SHOWINFO);
	(void) exEditFile(window, force, files[++curfile]);
	move_window_to_cursor(window);
	xvUpdateAllBufferWindows(window->w_buffer);
    } else {
	show_message(window, "No more files");
    }
    echo = savecho;
}

/*ARGSUSED*/
void
exRewind(window, force)
Xviwin	*window;
bool_t	force;
{
    unsigned	savecho;

    if (numfiles <= 1)		/* nothing to rewind */
	return;

    curfile = 0;

    savecho = echo;
    echo &= ~(e_SCROLL | e_REPORT | e_SHOWINFO);
    (void) exEditFile(window, force, files[0]);
    move_window_to_cursor(window);
    xvUpdateAllBufferWindows(window->w_buffer);
    echo = savecho;
}

/*
 * Append the buffer to the given filename,
 * from "line1" to "line2", forcing if necessary.
 *
 * If no filename given, use the buffer's filename.
 */
bool_t
exAppendToFile(window, filename, l1, l2, force)
Xviwin	*window;
char	*filename;
Line	*l1, *l2;
bool_t	force;
{
    if (filename == NULL) {
	filename = window->w_buffer->b_filename;
    }
    if (filename == NULL) {
	show_error(window, "No output file");
	return(FALSE);
    }

    return(appendit(window, filename, l1, l2, force));
}

/*
 * Write out the buffer, to the given filename,
 * from "line1" to "line2", forcing if necessary.
 *
 * If no filename given, use the buffer's filename.
 */
bool_t
exWriteToFile(window, filename, l1, l2, force)
Xviwin	*window;
char	*filename;
Line	*l1, *l2;
bool_t	force;
{
    register Buffer	*buffer;

    buffer = window->w_buffer;

    if (filename == NULL) {
	filename = buffer->b_filename;
    } else if (filename[0] == '!') {
	xvWriteToCommand(window, filename + 1, l1, l2);
	return(TRUE);
    }

    if (filename == NULL) {
	show_error(window, "No output file");
	return(FALSE);
    }

    /*
     * If we are writing the while buffer out to a specified file,
     * and the buffer is currently unnamed, we should give it the
     * name of the file (on the grounds that we are effectively now
     * editing that file).
     */
    if (
	buffer->b_filename == NULL
	&& 
	(l1 == NULL || is_line0(l1->l_prev))
	&&
	(l2 == NULL || is_lastline(l2->l_next))
    ) {
	buffer->b_filename = strsave(filename);
	if (buffer->b_tempfname != NULL) {
	    free(buffer->b_tempfname);
	}
	buffer->b_tempfname = NULL;
    }

    return(writeit(window, filename, l1, l2, force));
}

/*
 * Write to the given filename then quit.
 */
void
exWQ(window, filename, force)
Xviwin	*window;
char	*filename;
bool_t	force;
{
    if (exWriteToFile(window, filename, (Line *) NULL, (Line *) NULL, force)) {
	exQuit(window, force);	
    }
}

/*
 * Read the given file into the buffer after the specified line.
 * The line may not be NULL, but should be a line in the buffer
 * referenced by the passed window parameter.
 */
void
exReadFile(window, filename, atline)
Xviwin	*window;
char	*filename;
Line	*atline;
{
    Line	*head;		/* start of list of lines */
    Line	*tail;		/* last element of list of lines */
    long	nlines;		/* number of lines read */

    if (filename == NULL) {
	/*
	 * Default to the current filename if none specified.
	 * This seems odd, but vi does it, so why not.
	 */
	filename = window->w_buffer->b_filename;
    }

    if (filename[0] == '!') {
	if (filename[1] == '\0') {
	    show_error(window, "No shell command specified!");
	} else {
	    xvReadFromCommand(window, filename + 1, atline);
	}
	return;
    }

    nlines = get_file(window, filename, &head, &tail, "", " No such file");

    /*
     * If nlines > 0, we need to insert the lines returned into
     * the buffer. Otherwise, either the file is empty or an error
     * message has already been printed: in either case, we don't
     * need to do anything.
     */
    if (nlines > 0) {
	/*
	 * We want to see the message printed by
	 * get_file() here, not the message printed by
	 * report().
	 */
	echo &= ~e_REPORT;
	repllines(window, atline->l_next, 0L, head);
	echo |= e_REPORT;
	xvUpdateAllBufferWindows(window->w_buffer);

	/*
	 * Move the cursor to the first character
	 * of the file we just read in.
	 */
	move_cursor(window, atline->l_next, 0);
	begin_line(window, TRUE);
    }
}

/*
 * Edit alternate file. Called when control-^ is typed.
 * Use a new window for the edit only if autosplit allows.
 */
void
exEditAlternateFile(window)
Xviwin	*window;
{
    unsigned	savecho;
    bool_t	result;

    if (alt_file_name() == NULL) {
	show_error(window, "No alternate file to edit");
	return;
    }

    savecho = echo;
    echo &= ~e_SCROLL;

    result = xvCanSplit(window) ?
		exNewBuffer(window, alt_file_name(), 0) :
		exEditFile(window, FALSE, alt_file_name());
    if (result) {
	move_window_to_cursor(curwin);
	redraw_window(curwin, FALSE);
    }
    echo = savecho;
}

void
exShowFileStatus(window, newfile)
Xviwin	*window;
char	*newfile;
{
    Buffer	*buffer;

    buffer = window->w_buffer;

    if (newfile != NULL) {
	if (buffer->b_filename != NULL)
	    free(buffer->b_filename);
	buffer->b_filename = strsave(newfile);

	/*
	 * Remove the old preserve file, and call preserve() to try and
	 * preserve the new one. This may not immediately work, depending
	 * on the setting of preserve, but it's worth the call.
	 */
	unpreserve(buffer);
	(void) preservebuf(window);
    }
    show_file_info(window, TRUE);
}

void
exCompareWindows()
{
    Xviwin		*w;
    enum mvtype	incres;
    Posn		pos1, pos2;

    w = xvNextDisplayedWindow(curwin);
    if (w == curwin) {
	show_error(curwin, "No other buffers to compare");
    } else {
	pos1 = *(curwin->w_cursor);
	pos2 = *(w->w_cursor);
	while ((incres = inc(&pos1)) == inc(&pos2)) {
	    if (incres == mv_EOL) {
		continue;
	    } else if (incres == mv_NOMOVE) {
		(void) dec(&pos1);
		(void) dec(&pos2);
		break;
	    } else {
		if (gchar(&pos1) != gchar(&pos2)) {
		    break;
		}
	    }
	}
	if (gchar(&pos1) == '\0' && pos1.p_index > 0) {
	    (void) dec(&pos1);
	}
	if (gchar(&pos2) == '\0' && pos2.p_index > 0) {
	    (void) dec(&pos2);
	}
	move_cursor(curwin, pos1.p_line, pos1.p_index);
	move_cursor(w, pos2.p_line, pos2.p_index);
	move_window_to_cursor(w);
	cursupdate(w);
	wind_goto(w);
    }
}

static bool_t
more_files()
{
    int	n;

    n = numfiles - (curfile + 1);
    if (n > 0) {
	show_error(curwin, "%d more file%s to edit", n, (n > 1) ? "s" : "");
	return(TRUE);
    } else {
	return(FALSE);
    }
}
