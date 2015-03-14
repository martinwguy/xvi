/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    main.c
* module function:
    Entry point for xvi; setup, argument parsing and signal handling.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

/*
 * References to the current cursor position, and the window
 * and buffer into which it references. These make the code a
 * lot simpler, but we have to be a bit careful to update them
 * whenever necessary.
 */
Buffer	*curbuf;
Xviwin	*curwin;

/*
 * Global variables.
 */
state_t		State = NORMAL;	/* This is the current state of the command */
				/* interpreter. */

unsigned	echo;		/*
				 * bitmap controlling the verbosity of
				 * screen output (see xvi.h for details).
				 */

int		indentchars;	/* number of chars indented on current line */

volatile unsigned char
		kbdintr;	/*
				 * global flag set when a keyboard interrupt
				 * is received
				 */

bool_t		imessage;	/*
				 * global flag to indicate whether we should
				 * display the "Interrupted" message
				 */

/*
 * Internal routines.
 */
static	void	usage P((void));

Xviwin *
xvi_startup(vs, argc, argv, envp)
VirtScr	*vs;
int	argc;
char	*argv[];
char	*envp;				/* init string from the environment */
{
    char	*tag = NULL;		/* tag from command line */
    char	*pat = NULL;		/* pattern from command line */
    long	line = -1;		/* line number from command line */
    char	**files;
    int		numfiles = 0;
    int		count;
    char	*env;

    /*
     * The critical path this code has to follow is quite tricky.
     * We can't really run the "env" string until after we've set
     * up the first screen window because we don't know what the
     * commands in "env" might do: they might, for all we know,
     * want to display something. And we can't set up the first
     * screen window until we've set up the terminal interface.
     *
     * Also, we can't read the command line arguments until after
     * we've run the "env" string because a "-s param=value" argument
     * should override any setting of that parameter in the environment.
     *
     * If any errors occur, we call the startup_error() function
     * to display error messages, and then return NULL. The system
     * implementation just has to ensure that this is okay.
     * This will normally mean doing a sys_endv() at the first call
     * to get out of visual mode, and then printing to stdout.
     */

    /*
     * Set up the first buffer and screen window.
     */
    curbuf = new_buffer();
    if (curbuf == NULL) {
	startup_error("Can't allocate buffer memory.\n");
	return(NULL);
    }
    curwin = xvInitWindow(vs);
    if (curwin == NULL) {
	startup_error("Can't allocate buffer memory.\n");
	return(NULL);
    }

    /*
     * Connect the two together.
     */
    xvMapWindowOntoBuffer(curwin, curbuf);

    /*
     * Initialise parameter module.
     */
    init_params();

    /*
     * Initialise yank/put module.
     */
    init_yankput();

    init_sline(curwin);

    /*
     * Save a copy of the passed environment string in case it was
     * obtained from getenv(), so that the subsequent call we make
     * to get the SHELL parameter value does not overwrite it.
     */
    if (envp != NULL) {
	env = strsave(envp);
    } else {
    	env = NULL;
    }

    /*
     * Try to obtain a value for the "shell" parameter from the
     * environment variable SHELL. If this is NULL, do not override
     * any existing value. The system interface code (sys_init()) is
     * free to set up a default value, and the initialisation string
     * in the next part of the startup is free to override both that
     * value and the one from the environment.
     */
    {
	char	*sh;

	sh = getenv("SHELL");
	if (sh != NULL) {
	    set_param(P_shell, sh);
	}
    }

    /*
     * Run any initialisation string passed to us.
     *
     * We can't really do this until we have set up the terminal
     * because we don't know what the initialisation string might do.
     */
    if (env != NULL) {
	register char	*ep;
	register bool_t	escaped = FALSE;

	/*
	 * Commands in the initialization string can be
	 * separated by '|' (or '\n'), but a literal '|' or
	 * '\n' can be escaped by a preceding '\\', so we have
	 * to process the string, looking for all three
	 * characters.
	 */
	for (ep = env; *ep;) {
	    switch (*ep++) {
	    case '\\':
		escaped = TRUE;
		continue;
	    case '|':
	    case '\n':
		if (escaped) {
		    register char *s, *d;

		    for (d = (s = --ep) - 1; (*d++ = *s++) != '\0'; )
			;
		} else {
		    ep[-1] = '\0';
		    exCommand(env, FALSE);
		    env = ep;
		}
		/* fall through ... */
	    default:
		escaped = FALSE;
	    }
	}
	if (ep > env) {
	    exCommand(env, FALSE);
	}
    }

    /*
     * Process the command line arguments.
     *
     * We can't really do this until we have run the "env" string,
     * because "-s param=value" on the command line should override
     * parameter setting in the environment.
     */
    for (count = 1;
	 count < argc && (argv[count][0] == '-' || argv[count][0] == '+');
								count++) {

	if (argv[count][0] == '-') {
	    switch (argv[count][1]) {
	    case 't':
		/*
		 * -t tag or -ttag
		 */
		if (numfiles != 0) {
		    usage();
		    return(NULL);
		}
		if (argv[count][2] != '\0') {
		    tag = &(argv[count][2]);
		} else if (count < (argc - 1)) {
		    count += 1;
		    tag = argv[count];
		} else {
		    usage();
		    return(NULL);
		}
		break;

	    case 's':
		/*
		 * -s param=value or
		 * -sparam=value
		 */
		if (argv[count][2] != '\0') {
		    argv[count] += 2;
		} else if (count < (argc - 1)) {
		    count += 1;
		} else {
		    usage();
		    return(NULL);
		}
		exSet(curwin, 1, &argv[count], FALSE);
		break;

	    default:
		usage();
		return(NULL);
	    }

	} else /* argv[count][0] == '+' */ {
	    char	nc;

	    /*
	     * "+n file" or "+/pat file"
	     */
	    if (count >= (argc - 1)) {
		usage();
		return(NULL);
	    }

	    nc = argv[count][1];
	    if (nc == '/') {
		pat = &(argv[count][2]);
	    } else if (nc == '$') {
		line = 0; /* sends us to last line of file */
	    } else if (is_digit(nc)) {
		line = atol(&(argv[count][1]));
	    } else if (nc == '\0') {
		line = 0;
	    } else {
		usage();
		return(NULL);
	    }
	    count += 1;
	    files = &argv[count];
	    numfiles = 1;
	}
    }

    /*
     * At this point, read in any tags files specified. We leave it
     * until as late as possible so that the tags parameter used is
     * the one the user wants.
     */
    tagInit();

    if (numfiles != 0 || tag != NULL) {
	/*
	 * If we found "-t tag", "+n file" or "+/pat file" on
	 * the command line, we don't want to see any more
	 * file names.
	 */
	if (count < argc) {
	    usage();
	    return(NULL);
	}
    } else {
	/*
	 * Otherwise, file names are valid.
	 */
	numfiles = argc - count;
	if (numfiles > 0) {
	    files = &(argv[count]);
	}
    }

    /*
     * Initialise the cursor.
     */
    move_cursor(curwin, curbuf->b_file, 0);
    curwin->w_col = 0;
    curwin->w_row = 0;

    /*
     * Clear the window.
     *
     * It doesn't make sense to do this until we have a value for
     * Pn(P_colour).
     */
    xvClear(curwin->w_vs);

    if (numfiles != 0) {
	if (line < 0 && pat == NULL) {
	    echo = e_CHARUPDATE | e_SHOWINFO | e_ALLOCFAIL;
	}

	exNext(curwin, numfiles, files, FALSE);

	if (pat != NULL) {
	    Posn	*p;

	    echo = e_CHARUPDATE | e_SHOWINFO |
			e_REGERR | e_NOMATCH | e_ALLOCFAIL;

	    p = xvDoSearch(curwin, pat, '/');
	    if (p != NULL) {
		setpcmark(curwin);
		move_cursor(curwin, p->p_line, p->p_index);
		curwin->w_set_want_col = TRUE;
	    }
	} else if (line >= 0) {
	    echo = e_CHARUPDATE | e_SHOWINFO | e_ALLOCFAIL;
	    xvMoveToLineNumber((line > 0) ? line : MAX_LINENO);
	}

    } else if (tag != NULL) {
	echo = e_CHARUPDATE | e_SHOWINFO | e_REGERR | e_NOMATCH | e_ALLOCFAIL;
	if (exTag(curwin, tag, FALSE, TRUE, FALSE) == FALSE) {
	    /*
	     * Failed to find tag - wait for a while
	     * to allow user to read tags error and then
	     * display the "no file" message.
	     */
	    sleep(2);
	    show_file_info(curwin, TRUE);
	}
    } else {
	echo = e_CHARUPDATE | e_SHOWINFO | e_ALLOCFAIL;
	show_file_info(curwin, TRUE);
    }

    setpcmark(curwin);

    echo = e_CHARUPDATE | e_ALLOCFAIL;

    /*
     * Ensure we are at the right screen position.
     */
    move_window_to_cursor(curwin);

    /*
     * Redraw the screen.
     */
    redraw_all(curwin, FALSE);

    /*
     * Update the cursor position on the screen, and go there.
     */
    cursupdate(curwin);
    wind_goto(curwin);

    /*
     * Allow everything.
     */
    echo = e_ANY;

    if (env != NULL) {
	free(env);
    }

    return(curwin);
}

/*
 * Print usage message.
 *
 * This function is only called after we have set the terminal to vi mode.
 */
static void
usage()
{
    startup_error(Version);
    startup_error("\n\nUsage: xvi { options } [ file ... ]\n");
    startup_error("       xvi { options } -t tag\n");
    startup_error("       xvi { options } +[num] file\n");
    startup_error("       xvi { options } +/pat  file\n");
    startup_error("\nOptions are:\n");
    startup_error("       -s [no]boolean-parameter\n");
    startup_error("       -s parameter=value\n");
}
