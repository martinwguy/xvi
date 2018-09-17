/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    unix.c
* module function:
    System interface routines for all versions of UNIX.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Patched by P.T Breuer (ptb@eng.cam.ac.uk) to accept the HPUX
    flag for twiddles under HP-UX with gcc.
    Last modified by Martin Guy

***/

#include	"xvi.h"

/*
 * CTRL is defined by sgtty.h (or by a file it includes)
 * so we undefine it here to avoid conflicts with the
 * version defined in "xvi.h".
 */
#undef	CTRL

/* Without this we get compiler warning about conversion from int to
 * pointer, which could be damaging on a 64-bit system with 32-bit ints.
 * No idea why this is needed as stdio.h *is* included from xvi.h -martin */
FILE *fdopen(int fd, const char *mode);

/***********************************************************************
 *                    Environmental setup section                      *
 ***********************************************************************/

#ifndef SIGINT
#   include	<signal.h>		/* get signals for call_shell() */
#endif

#ifdef	BSD
#   include	<sys/wait.h>		/* get wait stuff for call_shell() */
    typedef	union wait Wait_t;
#else
    typedef	int	Wait_t;
#endif


#ifdef	POSIX
#	include <unistd.h>
#	include <sys/types.h>
#	include <sys/wait.h>
#endif

#if defined  _POSIX_C_SOURCE && _POSIX_C_SOURCE >= 200112L
#include	<sys/select.h>
#else
#include	<sys/time.h>
#endif

#if defined(sun) && !defined(POSIX)
#   ifndef TERMIOS
#	define	TERMIOS
#   endif
#endif

#ifdef TERMIOS
#   ifndef TERMIO
#	define	TERMIO
#   endif
#endif

#ifdef	TERMIO
#   ifdef	TERMIOS
#	include <termios.h>
#	ifndef TIOCGWINSZ
#	 include <sys/ioctl.h>  /* BSD and GNU/Linux need this for TIOCGWINSZ */
#	endif
   typedef struct termios	Termstate;
#	define getstate(p)	((void) tcgetattr(0, (p)))
#	define setstate(p)	((void) tcsetattr(0, TCSANOW, (p)))
#	define w_setstate(p)	((void) tcsetattr(0, TCSADRAIN, (p)))
#   else	/* no TERMIOS */
#	include <termio.h>
   typedef struct termio	Termstate;
#	define getstate(p)	((void) ioctl(0,TCGETA,(char *)(p)))
#	define setstate(p)	((void) ioctl(0,TCSETA,(char *)(p)))
#	define w_setstate(p)	((void) ioctl(0,TCSETAW,(char *)(p)))
#   endif	/* no TERMIOS */

#ifdef __hpux
#undef setstate
#endif

    /*
     * Table of line speeds ... exactly 16 long, and the CBAUD mask
     * is 017 (i.e. 15) so we will never access outside the array.
     */
# ifndef POSIX
#ifndef CBAUD
#	define CBAUD 017
#endif
    static short speeds[] = {
	/* B0 */	0,
	/* B50 */	50,
	/* B75 */	75,
	/* B110 */	110,
	/* B134 */	134,
	/* B150 */	150,
	/* B200 */	200,
	/* B300 */	300,
	/* B600 */	600,
	/* B1200 */	1200,
	/* B1800 */	1800,
	/* B2400 */	2400,
	/* B4800 */	4800,
	/* B9600 */	9600,
	/* EXTA */	19200,		/* not defined at V.2 */
#if 0
	/*
	 * This would be nice, but ospeed is defined to be a short,
	 * so there is just no way of legally including it.
	 */
	/* EXTB */	38400,		/* not defined at V.2 */
#else
	/* EXTB */	32767,		/* The best we can do */
#endif
    };
# endif /* !POSIX */

#else	/* not TERMIO */

#   include <sgtty.h>
    typedef struct sgttyb	Termstate;

#   ifdef TIOCGWINSZ
#	include <sys/ioctl.h>  /* For declaration of ioctl() */
#   endif

    static	struct	tchars	ckd_tchars, raw_tchars;
    static	struct	ltchars	ckd_ltchars, raw_ltchars;

#   ifdef FD_SET
#	define	fd_set_type	fd_set
#   else		/* FD_SET not defined */
	/*
	 * BSD 4.2 doesn't have these macros.
	 */
	typedef int fd_set_type;
#	define	FD_ZERO(p)	(*(p) = 0)
#	define	FD_SET(f,p)	(*(p) |= (1 << (f)))
#   endif		/* FD_SET not defined */
#endif	/* not TERMIO */

/*
 * CTRL is defined by sgtty.h (or by a file it includes)
 * so we undefine it here to avoid conflicts with the
 * version defined in "xvi.h".
 */
#undef	CTRL

/***********************************************************************
 *                END of environmental setup section                   *
 ***********************************************************************/

static	Termstate	cooked_state, raw_state;

#ifdef	SETVBUF_AVAIL
    /*
     * Output buffer to save function calls.
     */
    static	char	outbuffer[128];
#endif

/*
 * Expected for termcap's benefit.
 */
#ifndef AIX
extern short		ospeed;			/* tty's baud rate */
#endif

/*
 * We sometimes use a lot of system calls while trying to read from
 * the keyboard; these are needed to make our automatic buffer
 * preservation and input timeouts work properly. Nevertheless, it
 * is possible that, with this much overhead, a reasonably fast typist
 * could get ahead of us, so we do a small amount of input buffering
 * to reduce the number of system calls.
 *
 * This variable gives the number of characters in the buffer.
 */
static int	kb_nchars;

/*
 * The current input timeout to use for checking for pending
 * keyboard input.
 */
static long current_timeout = DEF_TIMEOUT;

/*
 * If this variable is TRUE, we can call subshells.
 * Set to TRUE in sys_init().
 */
bool_t	subshells = FALSE;

volatile bool_t	win_size_changed = FALSE;

/*
 * Get a single byte from the keyboard.
 *
 * If the keyboard input buffer is empty and read() times out, return EOF.
 *
 * POSIX: "If a read from the standard input returns an error, or if
 * the editor detects an end-of-file condition from the standard input,
 * it shall be equivalent to a SIGHUP asynchronous event."
 */
static int
kbgetc()
{
    static unsigned char	kbuf[48];
    static unsigned char	*kbp;

    if (kbdintr)
	return EOF;

    if (kb_nchars <= 0) {
        fd_set rfds;
        struct timeval tv;
        int retval;
        int nread;

	/* Wait until there is some input to be read */
        FD_ZERO(&rfds);
        FD_SET(0, &rfds);
        while (1) {
	    tv.tv_sec = (long) (current_timeout / 1000);
	    tv.tv_usec = ((long) current_timeout * 1000) % (long) 1000000;
            retval = select(1, &rfds, NULL, NULL, &tv);
            if (retval > 0)
                 break;
            if (retval == 0 || kbdintr)
                return EOF;
        }

	if ((nread = read(0, (char *) kbuf, sizeof kbuf)) <= 0) {
	    SIG_user_disconnected = TRUE;
	    return EOF;
	} else {
	    kb_nchars = nread;
	    kbp = kbuf;
	}
    }
    if (win_size_changed) {
	/*
	 * On some systems, a signal arriving will not cause the read() above
	 * to return EOF as the call will be restarted. So if we read chars from
	 * the input but a window size change has occurred, we should return EOF
	 * and hold off the characters until it has been processed.
	 */
	return(EOF);
    }
    --kb_nchars;
    return(*kbp++);
}

/*
 * Get a character from the keyboard.
 *
 * Make sure screen is updated first.
 */
int
inch(timeout)
long	timeout;
{
    int		c;

    /*
     * If we had characters left over from last time, return one.
     *
     * Note that if this happens, we don't call flush_output().
     */
    if (kb_nchars > 0) {
	return(kbgetc());
    }

    /*
     * Need to get a character. First, flush output to the screen.
     */
    (void) fflush(stdout);

    if (timeout != 0) {
	current_timeout = timeout;
	c = kbgetc();
	current_timeout = DEF_TIMEOUT;
	return(c);
    }

    /*
     * Keep trying until we get at least one character,
     * or we are interrupted.
     */

    return(kbgetc());
}

#ifdef	NEED_STRERROR
const char *
strerror(err)
int	err;
{
    extern char	*sys_errlist[];
    extern int	sys_nerr;

    return(
	err == 0 ?
	    "No error"
	    :
	    (err > 0 && err < sys_nerr) ?
		sys_errlist[err]
		:
		"Unknown error"
    );
}
#endif /* NEED_STRERROR */

static int
runvp(args)
char	**args;
{
    int		pid;
    Wait_t	status;

    (void) fflush(stdout);
    (void) fflush(stderr);
    pid = fork();
    switch (pid) {
    case -1:		/* fork() failed */
	return(-1);

    case 0:		/* this is the child */
    {
	char c;

	(void) signal(SIGINT, SIG_DFL);
	(void) signal(SIGQUIT, SIG_DFL);
	(void) execvp(args[0], args);

	/*
	 * Couldn't do it ... use standard output functions here
	 * because none of xvi's higher-level functions are usable
	 * from this module.
	 */
	(void) fputs("\007Can't execute ", stdout);
	(void) fputs(args[0], stdout);
	(void) fputs("\n(", stdout);
	(void) fputs(strerror(errno), stdout);
	(void) fputs(")\nHit return to continue ", stdout);
	(void) fflush(stdout);
	while (read(0, &c, 1) == 1 && c != '\n') {
	    ;
	}
	_exit(1);
    }
    default:		/* this is the parent */
	while (wait(&status) != pid)
	    ;
	return(status);
    }
}

int
call_shell(sh)
char	*sh;
{
    static char	*args[] = { NULL, NULL };

    args[0] = sh;
    return(runvp(args));
}

int
call_system(command)
char	*command;
{
    static char	*args[] = { NULL, "-c", NULL, NULL };

    if (Ps(P_shell) == NULL) {
	(void) fputs("\007Can't execute command without SHELL parameter", stdout);
	return(-1);
    }
    args[0] = Ps(P_shell);
    args[2] = command;
    return(runvp(args));
}

#if defined(ITIMER_REAL) && !defined(cray)
#   if defined(POSIX) || defined(HPUX) || defined(AIX)
	static void nothing P((int i)) { }
#   else
	static int  nothing() { return(0); }
#   endif
#endif

/*
 * Delay for a short time - preferably less than 1 second.
 * This is for use by showmatch, which wants to hold the
 * cursor over the matching bracket for just long enough
 * that it will be seen.
 */
void
Wait200ms()
{
#if defined(ITIMER_REAL) && !defined(cray)
    struct itimerval	timer;

    (void) signal(SIGALRM, nothing);

    /*
     * We want to pause for 200 msec (1/5th of a second) here,
     * as this seems like a reasonable figure. Note that we can
     * assume that the implementation will have defined tv_usec
     * of a type large enough to hold up to 999999, since that's
     * the largest number of microseconds we can possibly need.
     */
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 200000;
    if (setitimer(ITIMER_REAL, &timer, (struct itimerval *) NULL) == -1)
	return;
    (void) pause();

    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    (void) setitimer(ITIMER_REAL, &timer, (struct itimerval *) NULL);
#else /* not ITIMER_REAL */
    sleep(1);
#endif /* not ITIMER_REAL */
}

/*
 * Initialise the terminal so that we can do single-character
 * input and output, with no strange mapping, and no echo of
 * input characters.
 *
 * This must be done before any screen-based i/o is performed.
 */
void
sys_init()
{
    /*
     * What the device driver thinks the window's dimensions are.
     */
    unsigned int	rows = 0;
    unsigned int	columns = 0;

    /*
     * Set up tty flags in raw and cooked structures.
     * Do this before any termcap-initialisation stuff
     * so that start sequences can be sent.
     */
#ifdef	TERMIO

    getstate(&cooked_state);
    raw_state = cooked_state;

    raw_state.c_oflag &= ~(0
#ifdef   OPOST
			| OPOST
#else	/* no OPOST */
			/*
			 * Not a POSIX system. If it's a Sun, this might work:
			 */
			| OLCUC | OCRNL | ONLRET
#endif
#ifdef	ONLCR
			| ONLCR
#endif
#ifdef	OXTABS
			| OXTABS
#endif
#ifdef	XTABS
			| XTABS
#endif
#ifdef	TAB3
			| TAB3
#endif
    );

    raw_state.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL
#ifdef	XCASE
			| XCASE
#endif
#ifdef	ECHOCTL
			| ECHOCTL
#endif
#ifdef	ECHOPRT
			| ECHOPRT
#endif
#ifdef	ECHOKE
			| ECHOKE
#endif
    );

    raw_state.c_iflag &= ~(ICRNL | IGNCR | INLCR);
    raw_state.c_cc[VMIN] = 1;
    raw_state.c_cc[VTIME] = 0;
#   ifdef	TERMIOS
#	ifdef	    VDISCARD
	    raw_state.c_cc[VDISCARD] = 0;
#	endif
	raw_state.c_cc[VSUSP] = 0;
#   endif	/* TERMIOS */

    kbdintr_ch = raw_state.c_cc[VINTR];

#else	/* not TERMIO */

    /*
     * Probably a BSD system; we must
     *	turn off echo,
     *	set cbreak mode (not raw because we lose
     *		typeahead when switching modes),
     *	turn off tab expansion on output (in case termcap
     *		puts out any tabs in cursor motions etc),
     *	turn off CRMOD so we get \r and \n as they are typed,
     * and
     *	turn off nasty interrupt characters like ^Y so that
     *		we get the control characters we want.
     *
     * All this has to be put back as it was when we go into
     * system mode; a pain, but we have to get it right.
     */
    (void) ioctl(0, TIOCGETP, (char *) &cooked_state);
    raw_state = cooked_state;
    raw_state.sg_flags |= CBREAK;
    raw_state.sg_flags &= ~ (ECHO | XTABS | CRMOD);

    (void) ioctl(0, TIOCGETC, (char *) &ckd_tchars);
    raw_tchars = ckd_tchars;
    raw_tchars.t_quitc = -1;

    (void) ioctl(0, TIOCGLTC, (char *) &ckd_ltchars);
    raw_ltchars = ckd_ltchars;
    raw_ltchars.t_flushc = -1;
    raw_ltchars.t_lnextc = -1;
    raw_ltchars.t_suspc = -1;
    raw_ltchars.t_dsuspc = -1;

    kbdintr_ch = raw_tchars.t_intrc;

#endif	/* not TERMIO */

#ifdef	SETVBUF_AVAIL
    /*
     * Set output buffering to avoid calling _flsbuf()
     * for every character sent to the screen.
     */
    (void) setvbuf(stdout, outbuffer, _IOFBF, sizeof(outbuffer));
#endif

#ifndef AIX
    /*
     * This is for termcap's benefit.
     */
#ifdef	TERMIO
#   ifdef	POSIX
	ospeed = cfgetospeed(&cooked_state);
#   else	/* not POSIX */
	ospeed = speeds[cooked_state.c_cflag & CBAUD];
#   endif
#else
    ospeed = cooked_state.sg_ospeed;
#endif
#endif /* AIX */

    /*
     * Find out how big the kernel thinks the window is.
     * These values (if they are non-zero) will override
     * any settings obtained by the terminal interface code.
     */
    getScreenSize(&rows, &columns);

    /*
     * Now set up the terminal interface.
     */
    tty_open(&rows, &columns);

    /*
     * Go into raw/cbreak mode, and do any initialisation stuff.
     */
    sys_startv();

    subshells = TRUE;
}

void
getScreenSize(rp, cp)
unsigned int	*rp;
unsigned int	*cp;
{
    *rp = *cp = 0;

#ifdef	TIOCGWINSZ
    {
	struct winsize	winsz;
	if (ioctl(0, TIOCGWINSZ, (char *) &winsz) == 0) {
	    *rp = winsz.ws_row;
	    *cp = winsz.ws_col;
	}
    }
#endif

    /* Environment variables override all other kinds of screen size */
    {
	char *s;
	if ((s = getenv("LINES")) != NULL && isdigit(s[0])) {
	    *rp = (unsigned int) atol(s);
	}
	if ((s = getenv("COLUMNS")) != NULL && isdigit(s[0])) {
	    *cp = (unsigned int) atol(s);
	}
    }
}

static enum { m_SYS = 0, m_VI = 1 } curmode;

/*
 * Set terminal into the mode it was in when we started.
 *
 * sys_endv() can be called when we're already in system mode, so we
 * have to check.
 */
void
sys_endv()
{
    if (curmode == m_SYS)
	return;
    tty_goto((int) Rows - 1, 0);
    erase_line();
    tty_endv();

    (void) fflush(stdout);

    /*
     * Restore terminal modes.
     */
#ifdef	TERMIO
    w_setstate(&cooked_state);
#else
    (void) ioctl(0, TIOCSETN, (char *) &cooked_state);
    (void) ioctl(0, TIOCSETC, (char *) &ckd_tchars);
    (void) ioctl(0, TIOCSLTC, (char *) &ckd_ltchars);
#endif
    curmode = m_SYS;
}

/*
 * Set terminal to raw/cbreak mode.
 */
void
sys_startv()
{
    if (curmode == m_VI)
	return;
#ifdef	TERMIO
    w_setstate(&raw_state);
#else
    (void) ioctl(0, TIOCSETN, (char *) &raw_state);
    (void) ioctl(0, TIOCSETC, (char *) &raw_tchars);
    (void) ioctl(0, TIOCSLTC, (char *) &raw_ltchars);
#endif

    tty_startv();

    curmode = m_VI;
}

/*
 * "Safe" form of exit - doesn't leave the tty in yillruction mode.
 */
void
sys_exit(code)
int	code;
{
    sys_endv();
    tty_close();
    (void) fclose(stdin);
    (void) fclose(stdout);
    (void) fclose(stderr);

    /*
     * This is desperation really.
     * On some BSD systems, calling exit() here produces a core dump.
     */
    _exit(code);
}

/*
 * This is a routine that can be passed to tputs() (in the termcap
 * library): it does the same thing as putchar().
 */
int
foutch(c)
int	c;
{
    return(putchar(c));
}

/*
 * Construct unique name for temporary file, to be used as a backup
 * file for the named file.
 */
char *
tempfname(srcname)
char	*srcname;
{
    char	tailname[MAXNAMLEN + 1];
    char	*srctail;
    char	*endp;
    char	*retp;
    unsigned	headlen;
    unsigned	rootlen;
    unsigned	indexnum = 0;
    /* Where to put the preserve file if the source dir is unwritable */
    static char	*dir_prefix = NULL;	/* or "/tmp/" */

    if ((srctail = strrchr(srcname, '/')) == NULL) {
	srctail = srcname;
    } else {
	srctail++;
    }
    headlen = dir_prefix ? strlen(dir_prefix) : (srctail - srcname);

    /*
     * Make copy of filename's tail & change it from "wombat" to
     * "#wombat.tmp".
     */
    tailname [0] = '#';
    (void) strncpy(& tailname [1], srctail, sizeof tailname - 1);
    tailname [sizeof tailname - 1] = '\0';
    endp = tailname + strlen(tailname);

    /*
     * Don't let name get too long.
     */
    if (endp > & tailname [sizeof tailname - 5]) {
	endp = & tailname [sizeof tailname - 5];
    }
    rootlen = endp - tailname;

    /*
     * Now allocate space for new pathname.
     */
    retp = alloc(headlen + rootlen + 5);
    if (retp == NULL) {
	return(NULL);
    }

    /*
     * Copy name to new storage, leaving space for ".tmp"
     * (or ".xxx") suffix ...
     */
    if (headlen > 0) {
	(void) memcpy(retp, dir_prefix ? dir_prefix : srcname, (int) headlen);
    }
    (void) memcpy(&retp[headlen], tailname, (int) rootlen);

    /*
     * ... & make endp point to the space we're leaving for the suffix.
     */
    endp = &retp[headlen + rootlen];
    (void) strcpy(endp++, ".tmp");
    while (access(retp, 0) == 0) {
	/*
	 * Keep trying this until we get a unique file name.
	 */
	Flexbuf suffix;

	flexnew(&suffix);
	(void) lformat(&suffix, "%03u", ++indexnum);
	(void) strncpy(endp, flexgetstr(&suffix), 3);
	flexdelete(&suffix);
    }

    /* Make sure we can create the file. */
    {
	FILE *fp = fopenwb(retp);
	if ((fp ) != NULL) {
	    /* Yes. Delete it. */
	    fclose(fp);
	    remove(retp);
	} else if (dir_prefix == NULL) {
	    /* Put it in /tmp instead of the same dir as the source */
	    dir_prefix = "/tmp/";
	    retp = tempfname(srcname);
	    dir_prefix = NULL;
	}
    }
    return retp;
}

/*
 * This is like dup2(2), but it also closes the original file
 * descriptor.
 */
static void
dup2c(oldfd, newfd)
int oldfd;
int newfd;
{
    int tfd;

    (void) close(newfd);
    if ((tfd = dup(oldfd)) >= 0 && tfd < newfd)
	dup2c(tfd, newfd);
    (void) close(oldfd);
}

/*
 * Fork off children thus:
 *
 * [CHILD 1] --- pipe1 ---> [CHILD 2] --- pipe2 ---> [PARENT]
 *    |                        |                        |
 *    V                        V                        V
 * writefunc                exec(cmd)                readfunc
 *
 * connecting the pipes to stdin/stdout as appropriate.
 *
 * We assume that on entry to this function, file descriptors 0, 1 and 2
 * are already open, so we do not have to deal with the possibility of
 * them being allocated as pipe descriptors.
 */
bool_t
sys_pipe(cmd, writefunc, readfunc)
char	*cmd;
int	(*writefunc) P((FILE *));
long	(*readfunc) P((FILE *));
{
    int		pd1[2], pd2[2];		/* descriptors for pipes 1 and 2 */
    int		pid1, pid2;		/* process ids for children 1 and 2 */
    int		save0, save1, save2;	/* our saved files 0, 1 & 2 */
    int		died;
    int		retval;
    FILE	*fp;
    Wait_t	status;
    static char	*args[] = { NULL, "-c", NULL, NULL };

    if (Ps(P_shell) == NULL) {
	return(FALSE);
    }
    args[0] = Ps(P_shell);
    args[2] = cmd;

    /*
     * Initialise these values so we can work out what has happened
     * so far if we have to goto fail for any reason.
     */
    pd1[0] = pd1[1] = pd2[0] = pd2[1] =
	pid1 = pid2 = save0 = save1 = save2 = -1;

    /*
     * Set up pipe1, if we have been given a write function.
     * Otherwise, fake it by setting pd1[0] to standard input.
     */
    if (writefunc == NULL) {
	pd1[0] = dup(0);
    } else {
	if (pipe(pd1) == -1) {
	    goto fail;
	}

	/*
	 * Fork the writing side of the pipeline.
	 */
	switch (pid1 = fork()) {
	case -1:			/* error */
	    goto fail;

	case 0:				/* child 1 */
	    /*
	     * Fdopen pipe1 to get a stream.
	     */
	    (void) close(pd1[0]);
	    fp = fdopen(pd1[1], "w");
	    if (fp == NULL) {
		exit(1);
	    }

	    /*
	     * Call writefunc.
	     */
	    (void) (*writefunc)(fp);
	    (void) fclose(fp);

	    /*
	     * Our work is done.
	     */
	    exit(0);

	default:			/* parent */
	    (void) close(pd1[1]);
	    pd1[1] = -1;
	    break;
	}
    }

    /*
     * Set up pipe2, if we have been given a read function.
     */
    if (readfunc == NULL) {
	pd2[0] = dup(0);
	pd2[1] = dup(1);
    } else if (pipe(pd2) == -1) {
	goto fail;
    }

    if (
	 (save0 = dup(0)) == -1
	 ||
	 (save1 = dup(1)) == -1
	 ||
	 (save2 = dup(2)) == -1
    ) {
	goto fail;
    }

    (void) fflush(stdout);
    (void) fflush(stderr);

    /*
     * Fork the child; rearrange I/O streams; execute the command.
     */
    switch (pid2 = fork()) {
    case -1:				/* error */
	goto fail;

    case 0:				/* child 2 */
	(void) signal(SIGINT, SIG_DFL);
	(void) signal(SIGQUIT, SIG_DFL);

	/*
	 * Rearrange file descriptors onto stdin/stdout/stderr.
	 */
	(void) close(pd2[0]);
	(void) dup2c(pd1[0], 0);
	(void) dup2c(pd2[1], 1);
	(void) close(2);
	pid2 = dup(1);			/* dup2(1, 2) */

	/*
	 * Exec the command.
	 */
	(void) execvp(args[0], args);
	_exit(1);

    default:				/* parent */
	(void) close(pd1[0]);
	(void) close(pd2[1]);
	pd1[0] = pd2[1] = -1;
	if (readfunc != NULL) {
	    fp = fdopen(pd2[0], "r");
	    if (fp == NULL) {
		goto fail;
	    }
	    if ((*readfunc)(fp) < 0) {
		fclose(fp);
		goto fail;
	    }
	    (void) fclose(fp);
	} else {
	    (void) close(pd2[0]);
	}
	pd2[0] = -1;
	break;
    }

    /*
     * Finally, clean up the children.
     */
    retval = TRUE;
    goto cleanup;

fail:
    /*
     * Come here if anything fails (if we are the original process).
     * Close any open pipes and goto cleanup to reap the child processes.
     */
    retval = FALSE;

cleanup:
    /*
     * Come here whether or not we failed, to close any open pipes,
     * restore our standard input, output & error files & clean up the
     * children ...
     */

    if (pd1[0] >= 0) {
	(void) close(pd1[0]);
    }
    if (pd1[1] >= 0) {
	(void) close(pd1[1]);
    }
    if (pd2[0] >= 0) {
	(void) close(pd2[0]);
    }
    if (pd2[1] >= 0) {
	(void) close(pd2[1]);
    }
    if (save0 > 0) {
	dup2c(save0, 0);
    }
    if (save1 > 0) {
	dup2c(save1, 1);
    }
    if (save2 > 0) {
	dup2c(save2, 2);
    }

#if defined(WIFEXITED) && defined(WEXITSTATUS) && !defined(__hpux)
#   define	FAILED(s)	(!WIFEXITED(s) || WEXITSTATUS(s) != 0)
#else
#   define	FAILED(s)	((s) != 0)
#endif
#ifndef WTERMSIG
#   ifdef   WIFSIGNALED
#	define	WTERMSIG(s)	(WIFSIGNALED(s) ? (s).w_termsig : 0)
#   else
#	define	WTERMSIG(s)	((s) & 0177)
#   endif
#endif
    while ((died = wait(&status)) != -1) {
	if (died == pid1 || died == pid2) {
	    /*
	     * If child 1 was killed with SIGPIPE -
	     * because child 2 exited before reading all
	     * of its input - it isn't necessarily an
	     * error.
	     */
	    if (
		FAILED(status)
		&&
		(
		    died == pid2
		    ||
		    WTERMSIG(status) != SIGPIPE
		)
	    ) {
		retval = FALSE;
	    }
	}
    }

    return(retval);
}

/* Bit and pieces for fexpand */

static Flexbuf	newname;

/*
 * Returns the number of lines read, or -1 for failure.
 */
static long
readfunc(fp)
FILE *fp;
{
    register int	c;

    while ((c = getc(fp)) != EOF && c != '\n') {
	if (!flexaddch(&newname, c)) {
	    return(-1);
	}
    }
    return(1);
}

char *
fexpand(name, do_completion)
char			*name;
bool_t			do_completion;
{
    static char		meta[] = "*?[]~${}`";
    char		*cp;
    int			has_meta;
    static Flexbuf	cmd;
    char		*retval;

    has_meta = do_completion;
    for (cp = meta; *cp != '\0'; cp++) {
	if (strchr(name, *cp) != NULL) {
	    has_meta = TRUE;
	    break;
	}
    }
    if (!has_meta) {
	return(name);
    }

    if (Ps(P_shell) == NULL) {
	return(name);
    }

    retval = name;

    (void) fflush(stdout);
    (void) fflush(stderr);
    flexclear(&cmd);
    (void) lformat(&cmd, "echo %s%c", name, do_completion ? '*' : '\0');

    flexclear(&newname);

    if (sys_pipe(flexgetstr(&cmd), NULL, readfunc)) {
	if (!flexempty(&newname)) {
	    char *newstr;
	    int	  namelen;

	    newstr = flexgetstr(&newname);
	    namelen = strlen(name);
	    if (do_completion && strncmp(newstr, name, namelen) == 0 &&
				    newstr[namelen] == '*') {
		/*
		 * Unable to complete filename - return zero
		 * length string.
		 */
		retval = "";
	    } else {
		retval = newstr;
	    }
	}
    }
    return retval;
}
