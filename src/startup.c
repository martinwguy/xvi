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

#ifdef UNIX
# include <sys/types.h>		/* for getuid() and stat() */
# include <sys/stat.h>		/* for stat() */
#endif

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

int		indentchars;	/* number of chars used to indent current line */
int		next_indent;	/* auto-indent (in columns) for the next line */

volatile unsigned char
		kbdintr;	/*
				 * global flag set when a keyboard interrupt
				 * is received
				 */

char		kbdintr_ch = CTRL('C');	/* Key that sends an interrupt */

bool_t		imessage;	/*
				 * global flag to indicate whether we should
				 * display the "Interrupted" message
				 */

/*
 * Internal routines.
 */
static	void	usage P((void));

Xviwin *
xvi_startup(vs, argc, argv)
VirtScr	*vs;
int	argc;
char	*argv[];
{
    char	*envp;			/* init string from the environment */
    char	*tag = NULL;		/* tag from command line */
    char	*pat = NULL;		/* pattern from command line */
    long	line = 0;		/* line number from command line.
					 * 0 means unset or +0, first line
					 * MAX_LINENO means last line
					 * positive values are line N
					 * negative values are line $-N */
    char	**files;		/* Only valid if numfiles > 0 */
    int		numfiles = 0;
    int		count;
    char	*env = NULL;
    char	**commands = NULL;	/* Arguments to -c flags */
    int		ncommands = 0;		/* Arguments to -c flags */

    if (getenv("POSIXLY_CORRECT")) {
	set_param(P_posix, TRUE);
    }

    /*
     * Fetch the startup string.
     */
    envp = getenv("XVINIT");
    if (envp == NULL) {
	envp = getenv("EXINIT");
    }

    /*
     * Save a copy of the startup string so that subsequent calls
     * to getenv() do not overwrite it.
     */
    if (envp != NULL) {
	env = strsave(envp);
    }

#define add_command(c) { \
	commands = realloc(commands, sizeof(*commands) * ++ncommands); \
	commands[ncommands-1] = c; }

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
	startup_error(out_of_memory);
	return(NULL);
    }
    curwin = xvInitWindow(vs);
    if (curwin == NULL) {
	startup_error(out_of_memory);
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

    init_sline();

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
		    (void) exCommand(env);
		    env = ep;
		}
		/* fall through ... */
	    default:
		escaped = FALSE;
	    }
	}
	if (ep > env) {
	    (void) exCommand(env);
	}
    } else {
	/*
	 * POSIX: "If the EXINIT variable is not set,
	 *   the HOME environment variable is not null and not empty,
	 *   the file .exrc in the HOME directory:
	 *   - exists,
	 *   - is owned by the same user ID as the real user ID of the process
	 *     or the process has appropriate privileges [what DOES this mean?!]
	 *   - is not writable by anyone other than the owner
	 * the editor shall execute the ex commands contained in that file."
	 */
	char *home = getenv("HOME");

	if (home != NULL && home[0] != '\0') {
	    Flexbuf	flexrc;
	    char *	exrc;
#ifdef UNIX
	    struct stat	stbuf;
#endif

	    flexnew(&flexrc);
	    lformat(&flexrc, "%s/.exrc", home);
	    exrc = flexgetstr(&flexrc);
	    if (exists(exrc)
#ifdef UNIX
		&& stat(exrc, &stbuf) == 0 && stbuf.st_uid == getuid()
		&& (stbuf.st_mode & 022) == 0
#endif
			    ) {
		if (!exSource(exrc)) {
		    startup_error("Cannot read $HOME/.exrc");
		}
	    }
	    flexdelete(&flexrc);
	}
    }

    /*
     * POSIX: "If the current directory is not referred to by $HOME,
     *         a command in EXINIT or $HOME/.exrc sets the option exrc,
     *         the .exrc file in the current directory:
     *         - exists
     *         - is owned by the same user ID as the process' real user ID
     *         - is not writable by anyone other than the owner
     * the editor shall execute the ex commands contained in that file."
     *
     * What if $HOME is not set?
     */
    if (Pb(P_exrc)) {
	char *curdir;
	char *homedir;

	curdir = alloc(MAXPATHLEN + 1);
	if (curdir == NULL) {
	    startup_error(out_of_memory);
	    return(NULL);
	}
	if (getcwd(curdir, MAXPATHLEN + 1) != NULL
	    && (homedir = getenv("HOME")) != NULL && homedir[0] != '\0') {
	    /* On Unix, it would be more accurate to stat curdir and homedir
	     * and see whether the inode numbers correspond. */
	    if (strcmp(curdir, homedir) != 0) {
	        char *exrc = ".exrc";
#ifdef UNIX
		struct stat	stbuf;
#endif

		if (exists(exrc)
#ifdef UNIX
		    && stat(exrc, &stbuf) == 0 && stbuf.st_uid == getuid()
		    && (stbuf.st_mode & 022) == 0
#endif
			    ) {
		    if (!exSource(exrc)) {
			startup_error("Cannot read ./.exrc");
		    }
		}
	    }
	}
	free(curdir);
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
	    case 'c':
		if (count < argc-1) {
		    add_command(argv[++count]);
		} else {
		    usage();
		    return(NULL);
		}

		break;

	    case 'R':
		Pb(P_readonly) = TRUE;
		break;

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
		exSet(1, &argv[count]);
		break;

	    case 'w':	/* "window" size, not used in xvi */
		if (argv[count][2] != '\0') {		/* -wn */
		    ;
		} else if (count < (argc - 1)) {	/* -w n */
		    count += 1;
		} else {
		    usage();
		    return(NULL);
		}
		break;

	    default:
		usage();
		return(NULL);
	    }

	} else /* argv[count][0] == '+' */ {
	    /*
	     * "+n file" or "+/pat file"
	     */
	    if (count >= argc - 1 || !strchr("/$-0123456789", argv[count][1])) {
		usage();
		return(NULL);
	    }

	    /*
	     * Do +/ +$ +N as -c commands.  +- and +-N become $- and $-N
	     */
	    if (argv[count][1] == '-') {
		char *newarg = alloc(strlen(argv[count])+2);
		if (newarg) {
		    newarg[0] = '$';
		    strcpy(&newarg[1], &argv[count][1]);
		    add_command(newarg);
		}
	    } else {
		add_command(&argv[count][1]);
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
    move_cursor(curbuf->b_file, 0);
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
	if (line == 0 && pat == NULL) {
	    echo = e_CHARUPDATE | e_SHOWINFO | e_ALLOCFAIL;
	}

	/* At this point we want the status line updated with filenames */
	interactive = TRUE;

	(void) exNext(numfiles, files, FALSE);

	/* If there are more than one window, move to the first */
	while (curwin->w_last != NULL) {
	    curwin = curwin->w_last;
	}
	curbuf = curwin->w_buffer;
	xvUseWindow();

	if (pat != NULL) {
	    Posn	*p;

	    echo = e_CHARUPDATE | e_SHOWINFO |
			e_REGERR | e_NOMATCH | e_ALLOCFAIL;

	    p = xvDoSearch(pat, '/');
	    if (p != NULL) {
		setpcmark();
		move_cursor(p->p_line, p->p_index);
		curwin->w_set_want_col = TRUE;
	    }
	} else if (line != 0) {
	    echo = e_CHARUPDATE | e_SHOWINFO | e_ALLOCFAIL;
	    if (line < 0) {
		line = lineno(b_last_line_of(curbuf)) + line;
	    }
	    xvMoveToLineNumber(line);
	}

    } else if (tag != NULL) {
	echo = e_CHARUPDATE | e_SHOWINFO | e_REGERR | e_NOMATCH | e_ALLOCFAIL;
	if (exTag(tag, FALSE, TRUE, FALSE) == FALSE) {
	    /*
	     * Failed to find tag - wait for a while
	     * to allow user to read tags error and then
	     * display the "no file" message.
	     */
	    VSflush(curwin->w_vs);
	    sleep(2);
	    show_file_info(TRUE);
	}
    } else {
	echo = e_CHARUPDATE | e_SHOWINFO | e_ALLOCFAIL;
	show_file_info(TRUE);
    }

    /* Run any commands given with -c flag */
    if (commands != NULL) {
	int i;
	for (i=0; i < ncommands; i++) {
	    (void) exCommand(commands[i]);
	}
	free(commands);
    }

    setpcmark();

    echo = e_CHARUPDATE | e_ALLOCFAIL;

    /*
     * Ensure we are at the right screen position.
     */
    move_window_to_cursor();

    /*
     * Redraw the screen.
     */
    redraw_all(FALSE);

    /*
     * Update the cursor position on the screen, and go there.
     */
    cursupdate();
    wind_goto();

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
    startup_error("\nUsage: xvi { options } [ file ... ]");
    startup_error("       xvi { options } -t tag");
    startup_error("       xvi { options } +[num] file");
    startup_error("       xvi { options } +/pat  file");
    startup_error("\nOptions are:");
    startup_error("       -c ex-command");
    startup_error("       -R (set readonly mode)");
    startup_error("       -s [no]boolean-parameter");
    startup_error("       -s parameter=value");
    startup_error("       -w nlines (ignored)");
}
