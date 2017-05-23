/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    cmdline.c
* module function:
    Command-line handling (i.e. :/? commands) - most
    of the actual command functions are in ex_cmds*.c.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews	(onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

#ifdef	MEGAMAX
overlay "cmdline"
#endif

/*
 * The next two variables contain the bounds of any range
 * given in a command. If no range was given, both will be NULL.
 * If only a single line was given, u_line will be NULL.
 * The a_line variable is used for those commands which take
 * a third line specifier after the command, e.g. "move", "copy".
 */
static	Line	*l_line, *u_line;
static	Line	*a_line;

/*
 * Definitions for all ex commands.
 *
 * Unlike the parameters in param.h, these do not corrispond to the
 * order of the entries in cmdtable[] (multiple entries in cmdtable[]
 * have the same EX_COMMAND.)
 */

enum {
    EX_ENOTFOUND  = -1,		/* command not found */
    EX_EAMBIGUOUS = -2,		/* could be more than one */
    EX_ECANTFORCE = -3,		/* ! given where not appropriate */
    EX_EBADARGS	  = -4,		/* inappropriate args given */

    EX_NOCMD = 1,
    EX_SHCMD,
    EX_UNUSED,
    EX_AMPERSAND,
    EX_EXBUFFER,
    EX_LSHIFT,
    EX_EQUALS,
    EX_RSHIFT,
    EX_COMMENT,
    EX_ABBREVIATE,
    EX_APPEND,
    EX_ARGS,
    EX_BUFFER,
    EX_CHDIR,
    EX_CHANGE,
    EX_CLOSE,
    EX_COMPARE,
    EX_COPY,
    EX_DELETE,
    EX_ECHO,
    EX_EDIT,
    EX_EQUALISE,
    EX_EX,
    EX_FILE,
    EX_GLOBAL,
    EX_HELP,
    EX_INSERT,
    EX_JOIN,
    EX_K,
    EX_LIST,
    EX_MAP,
    EX_MARK,
    EX_MOVE,
    EX_NEXT,
    EX_NUMBER,
    EX_OPEN,
    EX_PRESERVE,
    EX_PRINT,
    EX_PUT,
    EX_QUIT,
    EX_READ,
    EX_RECOVER,
    EX_REWIND,
    EX_SET,
    EX_SHELL,
    EX_SOURCE,
    EX_SPLIT,
    EX_SUSPEND,
    EX_SUBSTITUTE,
    EX_TAG,
    EX_UNABBREV,
    EX_UNDO,
    EX_UNMAP,
    EX_V,
    EX_VERSION,
    EX_VISUAL,
    EX_WN,
    EX_WQ,
    EX_WRITE,
    EX_XIT,
    EX_YANK,
    EX_Z,
    EX_GOTO,
    EX_TILDE,
};

/*
 * Table of all ex commands, and whether they take an '!'.
 *
 * Note that this table is in strict order, sorted on
 * the ASCII value of the first character of the command.
 *
 * The priority field is necessary to resolve clashes in
 * the first one or two characters; so each group of commands
 * beginning with the same letter should have at least one
 * priority 1, so that there is a sensible default.
 *
 * Commands with argument type ec_rest need no delimiters;
 * they need only be matched. This is really only used for
 * single-character commands like !, " and &.
 */
static	struct	ecmd	{
    char		*ec_name;
    short		ec_command;
    unsigned char	ec_priority;
    unsigned char	ec_flags;
    /*
     * Flags: EXCLAM means can use !, FILEXP means do filename
     * expansion, INTEXP means do % and # expansion. EXPALL means
     * do INTEXP and FILEXP (they are done in that order).
     *
     * EC_RANGE0 means that the range specifier (if any)
     * may include line 0.
     */
#   define	EC_EXCLAM	0x1
#   define	EC_FILEXP	0x2
#   define	EC_INTEXP	0x4
#   define	EC_EXPALL	EC_FILEXP|EC_INTEXP
#   define	EC_RANGE0	0x8

    enum {
	ec_none,		/* no arguments after command */
	ec_strings,		/* whitespace-separated strings */
	ec_1string,		/* like ec_strings but only one */
	ec_filecmd,		/* may be a filename or a !command */
	ec_line,		/* line number or target argument */
	ec_rest,		/* rest of line passed entirely */
	ec_nonalnum,		/* non-alphanumeric delimiter */
	ec_1lower		/* single lower-case letter */
    }	ec_arg_type;
} cmdtable[] = {
/*  name		command	     priority	exclam	*/

    /*
     * The zero-length string is used for the :linenumber command.
     */
  { "",		    EX_NOCMD,	    1,	EC_RANGE0,		ec_none },
  { "!",	    EX_SHCMD,	    0,	EC_INTEXP,		ec_rest },

  { "#",	    EX_NUMBER,	    0,	0,			ec_none },
  { "&",	    EX_AMPERSAND,   0,	0,			ec_rest },
  { "*",	    EX_EXBUFFER,    0,	0,			ec_rest },
  { "<",	    EX_LSHIFT,	    0,	0,			ec_none },
  { "=",	    EX_EQUALS,	    0,	0,			ec_none },
  { ">",	    EX_RSHIFT,	    0,	0,			ec_none },
  { "@",	    EX_EXBUFFER,    0,	0,			ec_rest },
  { "\"",	    EX_COMMENT,	    0,	0,			ec_rest },

  { "abbreviate",   EX_ABBREVIATE,  0,	0,			ec_strings },
  { "append",	    EX_APPEND,	    1,	0,			ec_none },
  { "args",	    EX_ARGS,	    0,	0,			ec_none },

  { "buffer",	    EX_BUFFER,	    0,	EC_EXPALL,		ec_1string },

  { "cd",	    EX_CHDIR,	    0,	EC_EXPALL,		ec_1string },
  { "change",	    EX_CHANGE,	    1,	0,			ec_none },
  { "chdir",	    EX_CHDIR,	    0,	EC_EXPALL,		ec_1string },
  { "close",	    EX_CLOSE,	    0,	EC_EXCLAM,		ec_none },
  { "copy",	    EX_COPY,	    0,	0,			ec_line },

  { "delete",	    EX_DELETE,	    0,	0,			ec_none },

  { "echo",	    EX_ECHO,	    0,	EC_INTEXP,		ec_strings },
  { "edit",	    EX_EDIT,	    2,	EC_EXCLAM|EC_EXPALL,	ec_1string },
  { "equalise",	    EX_EQUALISE,    1,	0,			ec_none },
  { "equalize",	    EX_EQUALISE,    0,	0,			ec_none },
  { "ex",	    EX_EX,	    0,	EC_EXPALL,		ec_1string },

  { "file",	    EX_FILE,	    0,	EC_EXPALL,		ec_1string },

  { "global",	    EX_GLOBAL,	    0,	EC_EXCLAM,		ec_nonalnum },

  { "help",	    EX_HELP,	    0,	0,			ec_none },

  { "insert",	    EX_INSERT,	    0,	0,			ec_none },

  { "join",	    EX_JOIN,	    0,	EC_EXCLAM,		ec_none },

  { "k",	    EX_K,	    0,	0,			ec_1lower },

  { "list",	    EX_LIST,	    0,	0,			ec_none },

  /* "m" is move" but "ma" is "mark" */
  { "map",	    EX_MAP,	    0,	EC_EXCLAM,		ec_nonalnum },
  { "mark",	    EX_MARK,	    1,	0,			ec_1lower },
  { "move",	    EX_MOVE,	    2,	0,			ec_line },

  { "next",	    EX_NEXT,	    1,	EC_EXCLAM|EC_EXPALL,	ec_strings },
  { "number",	    EX_NUMBER,	    0,	0,			ec_none },

  { "open",	    EX_OPEN,	    0,	0,			ec_none },

  { "preserve",	    EX_PRESERVE,    0,	0,			ec_none },
  { "print",	    EX_PRINT,	    1,	0,			ec_none },
  { "put",	    EX_PUT,	    0,	EC_RANGE0,		ec_none },

  { "quit",	    EX_QUIT,	    0,	EC_EXCLAM,		ec_none },

  { "read",	    EX_READ,	    1,	EC_EXPALL|EC_RANGE0,	ec_filecmd },
  { "recover",	    EX_RECOVER,	    0,	0,			ec_none },
  { "rewind",	    EX_REWIND,	    0,	EC_EXCLAM,		ec_none },

  /* "s" is "substitute" but "su" is "suspend" */
  { "s",   	    EX_SUBSTITUTE,  0,	0,			ec_nonalnum },
  { "set",	    EX_SET,	    0,	0,			ec_strings },
  { "shell",	    EX_SHELL,	    0,	EC_EXCLAM,		ec_none },
  { "source",	    EX_SOURCE,	    0,	EC_EXPALL,		ec_1string },
  { "split",	    EX_SPLIT,	    0,	0,			ec_none },
  { "stop",	    EX_SUSPEND,	    0,	EC_EXCLAM,		ec_none },
  { "substitute",   EX_SUBSTITUTE,  0,	0,			ec_nonalnum },
  { "suspend",	    EX_SUSPEND,	    1,	EC_EXCLAM,		ec_none },

  { "t",	    EX_COPY,	    1,	0,			ec_line },
  { "tag",	    EX_TAG,	    0,	EC_EXCLAM,		ec_1string },

  { "unabbreviate", EX_UNABBREV,    0,	0,			ec_strings },
  { "undo",	    EX_UNDO,	    1,	0,			ec_none },
  { "unmap",	    EX_UNMAP,	    0,	EC_EXCLAM,		ec_strings },

  { "v",	    EX_V,	    1,	0,			ec_nonalnum },
  { "version",	    EX_VERSION,	    0,	0,			ec_none },
  { "visual",	    EX_VISUAL,	    0,	EC_EXCLAM|EC_EXPALL,	ec_1string },

  { "wn",	    EX_WN,	    0,	EC_EXCLAM,		ec_none },
  { "wq",	    EX_WQ,	    0,	EC_EXCLAM|EC_EXPALL,	ec_filecmd },
  { "write",	    EX_WRITE,	    1,	EC_EXCLAM|EC_EXPALL,	ec_filecmd },

  { "xit",	    EX_XIT,	    0,	0,			ec_none },

  { "yank",	    EX_YANK,	    0,	0,			ec_none },

  { "z",	    EX_Z,	    0,	0,			ec_none },

  { "|",	    EX_GOTO,	    0,	0,			ec_none },
  { "~",	    EX_TILDE,	    0,	0,			ec_rest },

  { NULL,	    0,		    0,	0,			ec_none },
};

/*
 * Internal routine declarations.
 */
static	int	decode_command P((char **, bool_t *, struct ecmd **));
static	bool_t	get_line P((char **, Line *, Line **));
static	bool_t	get_range P((char **));
static	void	badcmd P((char *));
static	char	*show_line P((void));
static	char	*expand_percents P((char *));

/*
 * These are used for display mode.
 */
static	Line	*curline;
static	Line	*lastline;
static	bool_t	do_line_numbers;

/*
 * Macro to skip over whitespace during command line interpretation.
 */
#define skipblanks(p)	{ while (is_space(*(p))) (p)++; }

/*
 * Should the parameter to a partial command be expanded if they press Tab?
 *
 * This repeats the initial code in exCommand() to parse a command line.
 *
 * Note: this says FALSE for file commands with bad address arguments
 * (e.g. a line number greater than the number of lines in the file,
 * a search pattern that is not found, etc) but doing better
 * needs more code that I'm willing to add for this.
 */
bool_t
should_fexpand(cmdline)
char *cmdline;
{
    bool_t	exclam;			/* true if ! was given */
    int		command;		/* which command it is */
    struct ecmd	*ecp;			/* ptr to command entry */

    while (*cmdline == ':') cmdline++;
    if (!get_range(&cmdline)) {
	 return(FALSE);
    }
    skipblanks(cmdline);
    command = decode_command(&cmdline, &exclam, &ecp);
    return (command > 0 && (ecp->ec_flags & EC_FILEXP) != 0);
}

/*
 * exCommand() - process a ':' command.
 *
 * The cmdline argument points to a complete command line to be processed
 * (this does not include the ':' itself).
 *
 * It returns TRUE if it succeeds, FALSE if it fails.
 */
bool_t
exCommand(cmdline)
char	*cmdline;			/* optional command string */
{
    char	*arg;			/* ptr to string arg(s) */
    int		argc = 0;		/* arg count for ec_strings */
    char	**argv = NULL;		/* arg vector for ec_strings */
    bool_t	exclam;			/* true if ! was given */
    int		command;		/* which command it is */
    struct ecmd	*ecp;			/* ptr to command entry */
    unsigned	savecho;		/* previous value of echo */
    int		error;			/* Did the command fail? */

    /*
     * Skip leading colons and blanks
     */
    while (cmdline[0] == ':') {
	++cmdline;
    }
    /* get_range() skips space characters after the : */

    /*
     * Parse a range, if present (and update the cmdline pointer).
     */
    if (!get_range(&cmdline)) {
	badcmd("Bad address range");
	return(FALSE);
    }

    /*
     * Decode the command.
     */
    skipblanks(cmdline);
    command = decode_command(&cmdline, &exclam, &ecp);

    if (command > 0) {
	/*
	 * Check that the range specified,
	 * if any, is legal for the command.
	 */
	if (!(ecp->ec_flags & EC_RANGE0)) {
	    if (l_line == curbuf->b_line0 || u_line == curbuf->b_line0) {
		badcmd("Specification of line 0 not allowed");
		return(FALSE);
	    }
	}

	switch (ecp->ec_arg_type) {
	case ec_none:
	    if (*cmdline == '!' && (ecp->ec_flags & EC_EXCLAM)) {
		cmdline++;
	    }
	    /* Only trailing whitespace is allowed */
	    skipblanks(cmdline);
	    if (*cmdline != '\0') {
		command = EX_EBADARGS;
	    }
	    break;

	case ec_line:
	    a_line = NULL;
	    skipblanks(cmdline);
	    if (!get_line(&cmdline, curwin->w_cursor->p_line, &a_line) ||
							a_line == NULL) {
		command = EX_EBADARGS;
	    }
	    break;

	case ec_1lower:
	    /*
	     * One lower-case letter.
	     */
	    skipblanks(cmdline);
	    if (!is_lower(cmdline[0]) || cmdline[1] != '\0') {
		command = EX_EBADARGS;
	    } else {
		arg = cmdline;
	    }
	    break;

	case ec_nonalnum:
	case ec_rest:
	case ec_strings:
	case ec_1string:
	case ec_filecmd:
	    arg = cmdline;

	    /*
	     * Null-terminate the command and skip whitespace
	     * to arg or end of line. Only do this for forced
	     * commands, it is not necessary (or desirable) for
	     * all commands as it would not allow recognition
	     * of e.g. ":write>>".
	     */
	    if (exclam && *arg == '!') {
		*arg++ = '\0';
	    }

	    switch (ecp->ec_arg_type) {
	    case ec_strings:
	    case ec_1string:
	    case ec_filecmd:
		skipblanks(arg);
		if (*arg == '\0') {
		    /*
		     * No args.
		     */
		    arg = NULL;
		}
	    default:
		break;
	    }

	    if (arg != NULL) {
		/*
		 * Perform expansions on the argument string.
		 */
		if (ecp->ec_flags & EC_INTEXP) {
		    arg = expand_percents(arg);
		    if (arg == NULL) return(FALSE);
		}
		if (ecp->ec_flags & EC_FILEXP) {
		    arg = fexpand(arg, FALSE);
		}

		switch (ecp->ec_arg_type) {
		case ec_filecmd:
		    if (*arg == '!') {
		        /* Write into pipe */
			break;
		    }
		    if (*arg == '>' && arg[1] == '>') {
		        /* Append to file */
			break;
		    }
		    /* FALL THROUGH */

		case ec_strings:
		case ec_1string:
		    makeargv(arg, &argc, &argv, " \t");
		    if (argv == NULL) return(FALSE);
		    if (ecp->ec_arg_type == ec_strings)
			break;

		    if (argc > 1) {
			/*
			 * Command can't take >1 argument.
			 */
			command = EX_EBADARGS;
		    }
		    arg = argv[0];	/* unnecessary but correct */

		default:
		    break;
		}
	    }
	}
    }

    savecho = echo;

    if (command == EX_GLOBAL || command == EX_V) {
	/*
	 * We have a "global" type command. This will be followed by
	 * another command which we have to execute for every line
	 * that matches the specified pattern. So first mark all the
	 * lines which match ...
	 */
	/*
	 * ... and then apply the command to each line in turn.
	 */
    }

    /*
     * Now do the command.
     */

    /*
     * If non-zero, some command failed so we need to abandon any
     * macros currently being executed.
     * Failed commands that call badcmd() don't need to set this.
     */
    error = 0;

    switch (command) {
    case EX_SHCMD:
	/*
	 * If a line range was specified, this must be a pipe command.
	 * Otherwise, it's just a simple shell command.
	 */
	if (l_line != NULL) {
	    specify_pipe_range(l_line, u_line);
	    if (!do_pipe(arg)) {
		error++;
	    }
	} else {
	    if (!exShellCommand(arg)) {
		error++;
	    }
	}
	break;

    case EX_ARGS:
	if (!exArgs()) {
	    error++;
	}
	break;

    case EX_BUFFER:
	if (arg != NULL)
	    echo &= ~(e_SCROLL | e_REPORT | e_SHOWINFO);
	if (!exNewBuffer(arg, 0)) {
	    error++;
	}
	move_window_to_cursor();
	redraw_window(FALSE);
	break;

    case EX_CHDIR:
    {
	char	*errmsg;

	if ((errmsg = exChangeDirectory(arg)) != NULL) {
	    badcmd(errmsg);
	    error++;
	} else if (interactive) {
	    char	*dirp;

	    if ((dirp = alloc(MAXPATHLEN + 2)) != NULL &&
		getcwd(dirp, MAXPATHLEN + 2) != NULL) {
		show_message("%s", dirp);
	    }
	    if (dirp) {
		free(dirp);
	    }
	}
	break;
    }

    case EX_CLOSE:
	if (!exCloseWindow(exclam)) {
	    error++;
	}
	break;

    case EX_COMMENT:		/* This one is easy ... */
	break;

    case EX_COPY:
	if (!exLineOperation('t', l_line, u_line, a_line)) {
	    error++;
	}
	break;

    case EX_DELETE:
	if (!exLineOperation('d', l_line, u_line, (Line *) NULL)) {
	    error++;
	}
	break;

    case EX_ECHO:			/* echo arguments on command line */
    {
	int	i;

	flexclear(&curwin->w_statusline);
	for (i = 0; i < argc; i++) {
	    if (!lformat(&curwin->w_statusline, "%s ", argv[i])
		    || flexlen(&curwin->w_statusline) >= curwin->w_ncols) {
		break;
	    }
	}
	update_sline();
	break;
    }

    case EX_EDIT:
    case EX_VISUAL:			/* treat :vi as :edit */
	echo &= ~(e_SCROLL | e_REPORT | e_SHOWINFO);
	if (!exEditFile(exclam, arg)) {
	    error++;
	}
	move_window_to_cursor();
	xvUpdateAllBufferWindows();
#if 0
	show_file_info(TRUE);
#endif
	break;

    case EX_EQUALISE:
	xvEqualiseWindows(0);
	break;

    case EX_FILE:
	exShowFileStatus(arg);
	break;

    case EX_GLOBAL:
	if (!exGlobal(l_line, u_line, arg, !exclam)) {
	    error++;
	}
	break;

    case EX_HELP:
	exHelp();
	break;

    case EX_JOIN:
	if (!exJoin(l_line, u_line, exclam)) {
	    error++;
	}
	break;

    case EX_MAP:
	if (!xvi_map(arg, exclam)) {
	    error++;
	}
	break;

    case EX_UNMAP:
	if (!xvi_unmap(argc, argv, exclam)) {
	    error++;
	}
	break;

    case EX_MARK:
    case EX_K:
    {
	Posn	pos;

	pos.p_index = 0;
	if (l_line == NULL) {
	    pos.p_line = curwin->w_cursor->p_line;
	} else {
	    pos.p_line = l_line;
	}
	if (!setmark(arg[0], curbuf, &pos)) {
	    error++;
	}
	break;
    }

    case EX_MOVE:
	if (!exLineOperation('m', l_line, u_line, a_line)) {
	    error++;
	}
	break;

    case EX_NEXT:
	/*
	 * exNext() handles turning off the appropriate bits
	 * in echo, & also calls move_window_to_cursor() &
	 * xvUpdateAllBufferWindows() as required, so we don't
	 * have to do any of that here.
	 */
	if (!exNext(argc, argv, exclam)) {
	    error++;
	}
	break;

    case EX_PRESERVE:
	if (!exPreserveAllBuffers()) {
	    error++;
	}
	break;

    case EX_LIST:
    case EX_PRINT:
    case EX_NUMBER:
	if (l_line == NULL) {
	    curline = curwin->w_cursor->p_line;
	    lastline = curline->l_next;
	} else if (u_line ==  NULL) {
	    curline = l_line;
	    lastline = l_line->l_next;
	} else {
	    curline = l_line;
	    lastline = u_line->l_next;
	}
	do_line_numbers = (Pb(P_number) || command == EX_NUMBER);
	disp_init(show_line, (int) curwin->w_ncols,
		  command == EX_LIST || Pb(P_list));
	break;

    case EX_PUT:
    {
	Posn	where;

	if (l_line != NULL) {
	    where.p_index = 0;
	    where.p_line = l_line;
	} else {
	    where.p_index = curwin->w_cursor->p_index;
	    where.p_line = curwin->w_cursor->p_line;
	}
	do_put(&where, FORWARD, '@', NULL);
	break;
    }

    case EX_QUIT:
	exQuit(exclam);
	break;

    case EX_REWIND:
	if (!exRewind(exclam)) {
	    error++;
	}
	break;

    case EX_READ:
	if (!exReadFile(arg, (l_line != NULL) ?
				l_line : curwin->w_cursor->p_line)) {
	    error++;
	}
	break;

    case EX_SET:
	exSet(argc, argv);
	break;

    case EX_SHELL:
	exInteractiveShell(exclam);
	break;

    case EX_SOURCE:
	if (arg == NULL) {
	    badcmd("Missing filename");
	} else if (!exSource(arg)) {
	    error++;
	}
	break;

    case EX_SPLIT:
	/*
	 * "split".
	 */
	exSplitWindow();
	break;

    case EX_SUBSTITUTE:
    case EX_AMPERSAND:
    case EX_TILDE:
    {
	long		nsubs;
	register long	(*func) P((Line *, Line *, char *));

	switch (command) {
	case EX_SUBSTITUTE:
	    func = exSubstitute;
	    break;
	case EX_AMPERSAND:
	    func = exAmpersand;
	    break;
	case EX_TILDE:
	    func = exTilde;
	}

	nsubs = (*func)(l_line, u_line, arg);
	if (nsubs == 0) {
	    error++;
	}
	xvUpdateAllBufferWindows();
	cursupdate();
	begin_line(TRUE);
	if (nsubs >= Pn(P_report)) {
	    show_message("%ld substitution%c",
			nsubs,
			(nsubs > 1) ? 's' : ' ');
	}
	break;
    }

    case EX_SUSPEND:
	exSuspend(exclam);
	break;

    case EX_TAG:
	if (!exTag(arg, exclam, TRUE, TRUE)) {
	    error++;
	}
	break;

    case EX_UNDO:
	undo();
	break;

    case EX_V:
	if (!exGlobal(l_line, u_line, arg, FALSE)) {
	    error++;;
	}
	break;

    case EX_VERSION:
	show_message(Version);
	break;

    case EX_WN:
	/*
	 * This is not a standard "vi" command, but it
	 * is provided in PC vi, and it's quite useful.
	 */
	if (exWriteToFile((char *)NULL, (Line *)NULL, (Line *)NULL, exclam)) {
	    /*
	     * See comment for EX_NEXT (above).
	     */
	    if (!exNext(0, argv, exclam)) {
		error++;
	    }
	} else {
	    error++;
	}
	break;

    case EX_WQ:
	exWQ(arg, exclam);
	if (State != EXITING) error++;
	break;

    case EX_WRITE:
	if (arg != NULL && arg[0] == '>') {
	    if (arg[1] == '>') {
		arg += 2;
		skipblanks(arg);
		if (arg[0] == 0) {
		    arg = NULL;
		}
	    } else {
		show_error("Write forms are 'w' and 'w>>'");
		error++;
		break;
	    }
	    if (!exAppendToFile(arg, l_line, u_line, exclam)) {
		error++;
	    }
	} else {
	    if (!exWriteToFile(arg, l_line, u_line, exclam)) {
		error++;
	    }
	}
	break;

    case EX_XIT:
	if (!exXit()) {
	    error++;
	}
	break;

    case EX_YANK:
	if (!exLineOperation('y', l_line, u_line, (Line *) NULL)) {
	    error++;
	}
	break;

    case EX_EXBUFFER:
	yp_stuff_input(arg[0], FALSE, FALSE);
	break;

    case EX_EQUALS:
	exEquals(l_line);
	break;

    case EX_LSHIFT:
    case EX_RSHIFT:
	if (l_line == NULL) {
	    l_line = curwin->w_cursor->p_line;
	}
	if (u_line == NULL) {
	    u_line = l_line;
	}
	tabinout((command == EX_LSHIFT) ? '<' : '>', l_line, u_line, NULL);
	begin_line(TRUE);
	xvUpdateAllBufferWindows();
	break;

    case EX_NOCMD:
	/*
	 * If we got a line, but no command, then go to the line.
	 */
	if (l_line != NULL) {
	    /* If they gave two addresses, go to the second one. */
	    if (u_line != NULL) l_line = u_line;

	    if (l_line == curbuf->b_line0) {
		l_line = l_line->l_next;
	    }
	    move_cursor(l_line, 0);
	    begin_line(TRUE);
	}
	break;

    case EX_ENOTFOUND:
	badcmd("Unrecognized command");
	break;

    case EX_EAMBIGUOUS:
	badcmd("Ambiguous command");
	break;

    case EX_ECANTFORCE:
	badcmd("Inappropriate use of !");
	break;

    case EX_EBADARGS:
	badcmd("Inappropriate arguments given");
	break;

    default:
	/*
	 * Decoded successfully, but unknown to us. Whoops!
	 */
	badcmd("Internal error - unimplemented command.");
	error++;
	break;

    case EX_ABBREVIATE:
    case EX_APPEND:
    case EX_CHANGE:
    case EX_EX:
    case EX_GOTO:
    case EX_INSERT:
    case EX_OPEN:
    case EX_RECOVER:
    case EX_UNABBREV:
    case EX_Z:
	badcmd("Unimplemented command.");
	error++;
	break;
    }

    if (error) {
	/* Halt any macros being executed */
	unstuff();
    }

    echo = savecho;

    if (argc > 0 && argv != NULL) {
	free(argv);
    }

    return(!(error || command < 0));
}

/*
 * Find the correct command for the given string, and return it.
 *
 * Updates string pointer to point to 1st char after command.
 *
 * Updates boolean pointed to by forcep according
 * to whether an '!' was given after the command;
 * if an '!' is given for a command which can't take it,
 * this is an error, and EX_ECANTFORCE is returned.
 * For unknown commands, EX_ENOTFOUND is returned.
 * For ambiguous commands, EX_EAMBIGUOUS is returned.
 *
 * Also updates *cmdp to point at entry in command table.
 */
static int
decode_command(str, forcep, cmdp)
char		**str;
bool_t		*forcep;
struct	ecmd	**cmdp;
{
    struct ecmd	*cmdptr;
    struct ecmd	*last_match = NULL;
    bool_t	last_exclam = FALSE;
    int		nmatches = 0;
    char	*last_delimiter = *str;

    for (cmdptr = cmdtable; cmdptr->ec_name != NULL; cmdptr++) {
	char	*name = cmdptr->ec_name;
	char	*strp;
	bool_t	matched;

	strp = *str;

	while (*strp == *name && *strp != '\0') {
	    strp++;
	    name++;
	}

	matched = (
	    /*
	     * we've got a full match, or ...
	     */
	    *strp == '\0'
	    ||
	    /*
	     * ... a whitespace delimiter, or ...
	     */
	    is_space(*strp)
	    ||
	    (
		/*
		 * ... at least one character has been
		 * matched, and ...
		 */
		name > cmdptr->ec_name
		&&
		(
		    (
			/*
			 * ... this command can accept a
			 * '!' qualifier, and we've found
			 * one ...
			 */
			(
			    (cmdptr->ec_flags & EC_EXCLAM)
			    ||
			    cmdptr->ec_arg_type == ec_filecmd
			)
			&&
			*strp == '!'
		    )
		    ||
		    (
			/*
			 * ... or it can take a
			 * non-alphanumeric delimiter
			 * (like '/') ...
			 */
			cmdptr->ec_arg_type == ec_nonalnum
			&&
			!is_alnum(*strp)
		    )
		    ||
		    (
			/*
			 * ... or it doesn't need any
			 * delimiter ...
			 */
			cmdptr->ec_arg_type == ec_rest
		    )
		    ||
		    (
			/*
			 * ... or we've got a full match,
			 * & the command expects a single
			 * lower-case letter as an
			 * argument, and we've got one ...
			 */
			cmdptr->ec_arg_type == ec_1lower
			&&
			*name == '\0'
			&&
			is_lower(*strp)
		    )
		    ||
		    (
			/*
			 * ... or we've got a partial match,
			 * and the command expects a line
			 * specifier as an argument, and the
			 * next character is not alphabetic.
			 */
			cmdptr->ec_arg_type == ec_line
			&&
			!is_alpha(*strp)
		    )
		    ||
		    (
			/*
			 * ... or we've got a partial match,
			 * the command can accept a filename,
			 * and the next character is not
			 * alphanumeric (stupid, but) ...
			 */
			(cmdptr->ec_flags & EC_INTEXP)
			&&
			!is_alnum(*strp)
		    )
		)
	    )
	);

	if (!matched)
	    continue;

	if (last_match == NULL ||
		(last_match != NULL &&
		 last_match->ec_priority < cmdptr->ec_priority)) {
	    /*
	     * No previous match, or this one is better.
	     */
	    last_match = cmdptr;
	    last_exclam = (*strp == '!');
	    last_delimiter = strp;
	    nmatches = 1;

	    /*
	     * If this is a complete match, we don't
	     * need to search the rest of the table.
	     */
	    if (*name == '\0')
		break;

	} else if (last_match != NULL &&
	       last_match->ec_priority == cmdptr->ec_priority) {
	    /*
	     * Another match with the same priority - remember it.
	     */
	    nmatches++;
	}
    }

    /*
     * For us to have found a good match, there must have been
     * exactly one match at a certain priority, and if an '!'
     * was used, it must be allowed by that match.
     */
    if (last_match == NULL) {
	return(EX_ENOTFOUND);
    } else if (nmatches != 1) {
	return(EX_EAMBIGUOUS);
    } else if (last_exclam &&
		! (last_exclam = (last_match->ec_flags & EC_EXCLAM)) &&
		! (last_match->ec_flags & EC_INTEXP)) {
	return(EX_ECANTFORCE);
    } else {
	*forcep = last_exclam;
	*str = last_delimiter;
	*cmdp = last_match;
	return(last_match->ec_command);
    }
}

/*
 * get_range - parse a range specifier
 *
 * Ranges are of the form
 *
 * ADDRESS1,ADDRESS2
 * ADDRESS		(equivalent to "ADDRESS,ADDRESS")
 * ADDRESS,		(equivalent to "ADDRESS,.")
 * ,ADDRESS		(equivalent to ".,ADDRESS")
 * ,			(equivalent to ".")
 * %			(equivalent to "1,$")
 *
 * where ADDRESS is
 *
 * /regular expression/ [INCREMENT]
 * ?regular expression? [INCREMENT]
 * $   [INCREMENT]
 * 'x  [INCREMENT]	(where x denotes a currently defined mark)
 * .   [INCREMENT]
 * INCREMENT		(equivalent to . INCREMENT)
 * number [INCREMENT]
 *
 * and INCREMENT is
 *
 * + number
 * - number
 * +			(equivalent to +1)
 * -			(equivalent to -1)
 * ++			(equivalent to +2)
 * --			(equivalent to -2)
 *
 * etc.
 *
 * The pointer *cpp is updated to point to the first character following
 * the range spec. If an initial address is found, but no second, the
 * upper bound is equal to the lower, except if it is followed by ','.
 *
 * Return FALSE if an error occurred, otherwise TRUE.
 */
static bool_t
get_range(cpp)
register char	**cpp;
{
    register char	*cp;
    char		sepchar;
    Line		*newstartline;

#define skipp	{ cp = *cpp; skipblanks(cp); *cpp = cp; }

    skipp;

    if (*cp == '%') {
	/*
	 * "%" is the same as "1,$".
	 */
	l_line = curbuf->b_file;
	u_line = b_last_line_of(curbuf);
	++*cpp;
	return TRUE;
    }

    /*
     * Clear the range variables.
     */
    l_line = NULL;
    u_line = NULL;

    if (!get_line(cpp, curwin->w_cursor->p_line, &l_line)) {
	return FALSE;
    }

    skipp;

    sepchar = *cp;
    if (sepchar != ',' && sepchar != ';') {
	u_line = l_line;
	return TRUE;
    }

    ++*cpp;

    if (l_line == NULL) {
	/*
	 * So far, we've got ",".
	 *
	 * ",address" is equivalent to ".,address".
	 */
	l_line = curwin->w_cursor->p_line;
    }

    if (sepchar == ';') {
	u_line = l_line;
	newstartline = (l_line != curbuf->b_line0) ? l_line : l_line->l_next;
    } else {
	newstartline = curwin->w_cursor->p_line;
    }

    skipp;
    if (!get_line(cpp, newstartline, &u_line)) {
	return FALSE;
    }
    if (u_line == NULL) {
	/*
	 * "address," is equivalent to "address,.".
	 */
	u_line = curwin->w_cursor->p_line;
    }

    /*
     * Check for reverse ordering.
     */
    if (earlier(u_line, l_line)) {
	Line	*tmp;

	tmp = l_line;
	l_line = u_line;
	u_line = tmp;
    }

    skipp;
    return TRUE;
}

/*
 * Parse a single line specifier. The cpp argument is a reference to the
 * line specifier text; this pointer is updated to point to the character
 * after the specifier; startline is the line position within curwin from
 * which line specifiers should be relative; and the lpp argument is a
 * reference to a pointer which is updated to the value of the line which
 * was specified, if we find a valid specifier.
 *
 * Returns TRUE if a line specifier was found, updating *cpp to point to
 * the character after the line specifier, FALSE otherwise.
 */
static bool_t
get_line(cpp, startline, lpp)
    char		**cpp;
    Line		*startline;
    Line		**lpp;
{
    Line		*pos;
    char		*cp;
    char		c;
    long		lnum;

    cp = *cpp;

    /*
     * Determine the basic form, if present.
     */
    switch (c = *cp++) {

    case '/':
    case '?':
	pos = linesearch(startline, (c == '/') ? FORWARD : BACKWARD, &cp);
	if (pos == NULL) {
	    return FALSE;
	}
	break;

    case '$':
	pos = b_last_line_of(curbuf);
	break;

	/*
	 * "+n" is equivalent to ".+n".
	 */
    case '+':
    case '-':
	cp--;
	 /* fall through ... */
    case '.':
	pos = startline;
	break;

    case '\'':
    case '`':
    {
	Posn *posn = getmark(*cp++, curbuf);
	if (posn == NULL) {
	    show_error("Unknown mark");
	    return FALSE;
	}
	pos = posn->p_line;
	break;
    }
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
	for (lnum = c - '0'; is_digit(*cp); cp++)
	    lnum = lnum * 10 + *cp - '0';

	if (lnum == 0) {
	    pos = curbuf->b_line0;
	} else {
	    /* Going to a line number past the end of the buffer is an error */
	    if (lnum > lineno(b_last_line_of(curbuf))) {
		return FALSE;
	    }
	    pos = gotoline(curbuf, (unsigned long) lnum);
	}
	break;
	
    default:
	return TRUE;
    }

    skipblanks(cp);

    if (*cp == '-' || *cp == '+') {
	char		dirchar = *cp++;
	unsigned long	target;

	skipblanks(cp);
	if (is_digit(*cp)) {
	    for (lnum = 0; is_digit(*cp); cp++) {
		lnum = lnum * 10 + *cp - '0';
	    }
	} else {
	    for (lnum = 1; *cp == dirchar; cp++) {
		lnum++;
	    }
	}

	if (dirchar == '-') {
	    target = lineno(pos) - lnum;
	} else {
	    target = lineno(pos) + lnum;
	}

	if (target < 1 || target > lineno(b_last_line_of(curbuf))) {
	    return FALSE;
	}

	pos = gotoline(curbuf, target);
    }

    *cpp = cp;
    *lpp = pos;
    return TRUE;
}

/*
 * This routine is called for ambiguous, unknown,
 * badly defined or unimplemented commands.
 */
static void
badcmd(str)
char	*str;
{
    if (interactive) {
	show_error(str);
    }
    /* Halt any macros being executed */
    unstuff();
}

static char *
show_line()
{
    Line	*lp;

    if (curline == lastline) {
	return(NULL);
    }

    lp = curline;
    curline = curline->l_next;

    if (do_line_numbers) {
	static Flexbuf	nu_line;

	flexclear(&nu_line);
	(void) lformat(&nu_line, NUM_FMT, lineno(lp));
	(void) lformat(&nu_line, "%s", lp->l_text);
	return flexgetstr(&nu_line);
    } else {
	return(lp->l_text);
    }
}

static char *
expand_percents(str)
char	*str;
{
    static Flexbuf	newstr;
    register char	*from;
    register bool_t	escaped;

    if (strpbrk(str, "%#") == (char *) NULL) {
	return str;
    }
    flexclear(&newstr);
    escaped = FALSE;
    for (from = str; *from != '\0'; from++) {
	if (!escaped) {
	    switch (*from) {
	    case '%':
		if (curbuf->b_filename == NULL) {
		    show_error("Current filename is not set");
		    return(NULL);
		}
		(void) lformat(&newstr, "%s", curbuf->b_filename);
		break;
	    case '#':
	        if (alt_file_name() == NULL) {
		    show_error("Alternate filename is not set");
		    return(NULL);
		}
		(void) lformat(&newstr, "%s", alt_file_name());
		break;
	    case '\\':
		escaped = TRUE;
		break;
	    default:
		if (!flexaddch(&newstr, *from)) return(NULL);
	    }
	} else {
	    if (*from != '%' && *from != '#') {
		if (!flexaddch(&newstr, '\\')) return(NULL);
	    }
	    if (!flexaddch(&newstr, *from)) return(NULL);
	    escaped = FALSE;
	}
    }
    return flexgetstr(&newstr);
}
