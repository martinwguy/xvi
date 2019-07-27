/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    xvi.h
* module function:
    General definitions for xvi.

    This file should really be split up into several files
    rather than being concentrated into one huge monolith.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

/***************************************************************
 *                                                             *
 * SECTION 0: PORTABILITY HACKS                                *
 *                                                             *
 * For xvi-local names that clash with those in AIX curses.h   *
 *                                                             *
 ***************************************************************/
#ifdef AIX
#define beep xvi_beep
#define echo xvi_echo
#define scroll xvi_scroll
#undef inch
#define inch xvi_inch
#undef newline
#define newline xvi_newline
#endif

/***************************************************************
 *                                                             *
 * SECTION 1: ENVIRONMENT                                      *
 *                                                             *
 * Most of this section is concerned with including the right  *
 * header files; we also define some things by hand if the     *
 * appropriate definitions are not provided by the system.     *
 *                                                             *
 ***************************************************************/

/*
 * System include files ...
 */
#include <stdio.h>
#include <signal.h>
#include <string.h>

/*
 * Type of an object whose pointers can be freely cast.
 */
#ifdef	__STDC__
#   define  genptr	void
#else
#   define  genptr	char
#endif

#ifdef	__STDC__

#   define  USE_STDHDRS
#   define  STRERROR_AVAIL
#   include <errno.h>
#   include <stdarg.h>
#   define  VA_START(a, b)	va_start(a, b)
#   define  P(args)		args

#else	/* not __STDC__ */

#   ifdef   ultrix
#	ifdef	mips
#	    define  USE_STDHDRS
#	endif   /* mips */
#   endif   /* ultrix */
#   ifdef   sparc
#	define	USE_STDHDRS
#   endif   /* sparc */
#   include <varargs.h>
#   define  VA_START(a, b)	va_start(a)

#   define  P(args)		()

#   define  const
#   define  volatile

#endif	/* not __STDC__ */

#ifdef	USE_STDHDRS
#   include <stdlib.h>
#   include <stddef.h>
#   include <limits.h>
#else	/* USE_STDHDRS not defined */
    extern  FILE	*fopen();
    extern  char	*malloc();
    extern  char	*realloc();
    extern  char	*getenv();
    extern  long	atol();
    extern  char	*memcpy();
    extern  char	*memset();
    extern  void	exit();
#endif	/* USE_STDHDRS not defined */

#undef USE_STDHDRS

/*
 * Functions which ANSI does not specify should be included in
 * any standard header file. They should, however, be defined
 * on POSIX systems (by unix.h). Also, any makefile may define
 * POS_FUNCS if it knows that these functions will be declared.
 */
#ifdef	POSIX
#	define	POS_DECLS
#endif
#ifndef	POS_DECLS
    extern  int		chdir P((const char *path));
    extern  char	*getcwd P((char *, unsigned));
    extern  void	sleep P((unsigned seconds));
#endif

/*
 * A null function pointer definition. Some ANSI compilers complain
 * about comparison/assignment of function pointers with (void *) 0,
 * and since that's what NULL is usually defined as (under ANSI) we
 * use this to stop the warnings.
 */
#define	NOFUNC	0

/*
 * If we have ANSI C, these should be defined in limits.h:
 */
#ifndef INT_MAX
#   define INT_MAX	((int) ((unsigned int) ~0 >> 1))
#endif
#ifndef INT_MIN
#   define INT_MIN	(~INT_MAX)
#endif
#ifndef ULONG_MAX
#   define ULONG_MAX	0xffffffff
#endif

/*
 * Macro to convert a long to an int.
 * If a long is the same as an int, this is trivial.
 */
#define	LONG2INT(n)	(sizeof(int) == sizeof(long) ? (int) (n) : \
			 (int) ((n) > INT_MAX ? INT_MAX : \
			  ((n) < INT_MIN ? INT_MIN : (n))))

/*
 * Return value of "lval" unless it is 0, in which case return the
 * default value of 1. This is used for prefix-count code in normal.c.
 */
#define	LDEF1(val)	(((val) == 0 ? 1L : (val)))

/*
 * Return value of lval as an int, unless it is 0, in which case
 * return the default value of 1.
 *
 * Note that this assumes that val will never be negative.
 */
#define	IDEF1(val)	((val) == 0 ? \
			 1 : \
			 sizeof (int) == sizeof (long) || \
			   (val) <= INT_MAX ? \
			 (int) (val) : \
			 INT_MAX)


/***************************************************************
 *                                                             *
 * SECTION 2: FUNDAMENTAL TYPES                                *
 *                                                             *
 * These types are used by other included header files.        *
 *                                                             *
 ***************************************************************/

/*
 * Boolean type.
 * It would be possible to make this an enumerated type,
 * but it isn't worth the hassle - enums don't confer any
 * real advantages, and it means we can't do things like
 *
 *	bool_t	value = (i == 47);
 *
 * but instead are forced to write the abominable
 *
 *	bool_t	value = (i == 47) ? TRUE : FALSE;
 *
 * which is silly.
 */
#undef	FALSE			/* just in case */
#undef	TRUE
#define	FALSE		0
#define	TRUE		1
typedef	int		bool_t;


/***************************************************************
 *                                                             *
 * SECTION 3: FUNDAMENTAL HEADER FILES                         *
 *                                                             *
 * These header files define types which are used by Xvi.      *
 *                                                             *
 ***************************************************************/

#include "virtscr.h"


/***************************************************************
 *                                                             *
 * SECTION 4: MISCELLANEOUS DEFINITIONS                        *
 *                                                             *
 * Definitions of limits and suchlike used within the editor.  *
 *                                                             *
 ***************************************************************/

/*
 * Minimum number of rows a window can have, including status line.
 */
#define	MINROWS		2

/*
 * The number of characters taken up by the line number
 * when "number" is set; up to 6 digits plus two spaces.
 */
#define	NUM_SIZE	8
#define	NUM_FMT		"%6ld  "

/*
 * Maximum value for the tabstop parameter.
 */
#define	MAX_TABSTOP	32

/*
 * Default timeout for keystrokes (in milliseconds).
 * The timeout parameter is set to this value.
 */
#define	DEF_TIMEOUT	200


/***************************************************************
 *                                                             *
 * SECTION 5: PARAMETER TYPE DEFINITIONS                       *
 *                                                             *
 * These are definitions of types for particular parameters.   *
 *                                                             *
 ***************************************************************/

/*
 * Regular expression search modes - used in search.c.
 * These are the integer values to which the P_regextype
 * enumerated parameter may be set. Note that these values
 * are ordered; e.g. rt_GREP is considered to be earlier
 * than, and/or less than, rt_EGREP. If the types are added
 * to, this ordering must be maintained. Also note that the
 * names in the rt_strings table must follow the same order.
 */
#define rt_TAGS		0	/* only ^ and $ are significant */
#define rt_GREP		1	/* like grep, but with \< and \> */
#define rt_EGREP	2	/* like egrep, but with \< and \> */

/*
 * Array of names for the P_regextype enumeration, defined in
 * search.c.
 */
extern	char		*rt_strings[];

/*
 * Integer values for the P_preserve enumerated parameter. Note that
 * the names in psv_strings must follow the same order.
 */
#define psv_UNSAFE	0	/* never preserve buffer before writing */
#define psv_STANDARD	1	/*
				 * only preserve buffer before writing
				 * if it hasn't been preserved recently
				 */
#define psv_SAFE	2	/* always preserve buffer before writing */
#define psv_PARANOID	3	/*
				 * like psv_SAFE, but never remove the
				 * preserve file
				 */

/*
 * Array of names for the P_preserve enumeration, defined in
 * search.c.
 */
extern	char		*psv_strings[];

/*
 * Integer values for the P_format enumerated parameter. These are for
 * the formats we know about so far. Note that the entries in
 * fmt_strings & tftable (defined in fileio.c) must follow the same order.
 */
#define	fmt_CSTRING	0
#define	fmt_MACINTOSH	1
#define	fmt_MSDOS	2
#define	fmt_OS2		3
#define	fmt_QNX		4
#define	fmt_TOS		5
#define	fmt_UNIX	6

/*
 * Array of names for the P_format enumeration.
 */
extern	char		*fmt_strings[];

/*
 * Integer values for the P_jumpscroll enumerated parameter. Note that
 * the entries in js_strings (defined in param.c) must follow the same
 * order.
 */
#define	js_OFF		0
#define	js_AUTO		1
#define	js_ON		2

/*
 * Integer values for the P_infoupdate enumerated parameter. Note that
 * the entries in iu_strings (defined in param.c) must follow the same
 * order.
 */
#define	iu_TERSE	0
#define	iu_CONTINUOUS	1

/***************************************************************
 *                                                             *
 * SECTION 6: EDITOR TYPE DEFINITIONS                          *
 *                                                             *
 ***************************************************************/

/*
 * Possible editor states.
 */
typedef enum {
    NORMAL,			/* command mode */
    SUBNORMAL,			/* awaiting 2nd char of a 2-char NORMAL */
    INSERT,			/* insert mode */
    REPLACE,			/* overwrite mode */
    CMDLINE,			/* on the : command line */
    DISPLAY,			/* in display mode (e.g. g//p or :set all) */
    EXITING			/* bye bye */
} state_t;

extern	state_t		State;	/* defined in main.c */

/*
 * Possible return values for cmd_input(), which deals with command
 * line input. This is for commands starting with :, /, ? and !.
 */
typedef enum {
    cmd_COMPLETE,		/* user hit return (cmd line available) */
    cmd_INCOMPLETE,		/* not finished typing command line yet */
    cmd_CANCEL			/* user aborted command line */
} Cmd_State;

/*
 * Possible directions for searching, and opening lines.
 */
#define	FORWARD		0
#define	BACKWARD	1

/*
 * Line structure and its friends.
 *
 * The structure used to hold a line; this is used to form
 * a doubly-linked list of the lines comprising a file.
 *
 * The definition for MAX_LINENO depends on the type of
 * l_number, and on the size of the machine; we are fairly
 * safe setting all bits here so long as l_number is always
 * an unsigned type. Should really use a lineno_t here, and
 * get rid of the need for a maximum lineno.
 */
typedef	struct	line {
    struct line		*l_prev;	/* previous line */
    struct line		*l_next;	/* next line */
    char		*l_text;	/* text for this line */
    int			l_size;		/* actual size of space at 's' */
    unsigned long	l_number;	/* line "number" */
} Line;

#define	MAX_LINENO	ULONG_MAX

/*
 * These pseudo-functions operate on lines in a buffer, returning TRUE
 * or FALSE according to whether l1 is later or earlier than l2.
 * Note that there is no macro for "same", which is excluded by both
 * "earlier" and "later".
 */
#define	later(l1, l2)	((l1)->l_number > (l2)->l_number)
#define	earlier(l1, l2)	((l1)->l_number < (l2)->l_number)

/*
 * This macro gives the line number of line 'l' in buffer 'b'.
 */
#define	lineno(l)	((l)->l_number)

/*
 * Easy ways of finding out whether a given line is the first
 * or last line of a buffer, without needing a buffer pointer.
 */
#define	is_lastline(lp)	((lp)->l_number - MAX_LINENO == 0) /* i.e. == */
#define	is_line0(lp)	((lp)->l_number == 0)


/*
 * Structure used to hold a position in a file;
 * this is just a pointer to the line, and an index.
 */
typedef	struct	position {
    Line		*p_line;	/* line we're referencing */
    int			p_index;	/* position within that line */
} Posn;

/*
 * This structure is used to hold information about a command while
 * it is being collected and executed. It contains data to represent
 * a target position, and some flags which may alter how the target
 * is treated (some targets act differently depending on the operator
 * which is being applied).
 */
typedef	struct cmd {
    Posn	cmd_startpos;	/* From */
    Posn	cmd_target;	/* To */
    unsigned char cmd_flags;	/* Flags from its entry in cmd_types[] */
    bool_t	cmd_two_char;	/* Have we had the 1st char of a 2-char cmd? */
    int		cmd_operator;	/* An operator if we have one: NOP or !<>cdy */
    int		cmd_ch1;	/* First char of a two-char command */
    int		cmd_ch2;	/* Second char of a two-char command */
    long	cmd_prenum;	/* Repeat count for command */
    long	cmd_opnum;	/* Repeat count for operator */
    bool_t	cmd_got_yp_name;/* Command preceded by a buffer name? */
    int		cmd_yp_name;	/* Yank text into this buffer */
    int		cmd_buffer_name;/* if got_yp_name, they named this buffer */
} Cmd;

/*
 * This is stuff to do with marks - it should be privately defined
 * in mark.c, but it needs to be related to each individual buffer.
 */
#define	NMARKS	26		/* max. # of marks that can be saved */

typedef	struct	mark {
    Posn		m_posn;
    bool_t		m_deleted;
} Mark;

#include "change.h"

/*
 * Variable-length FIFO queue of characters.
 */
typedef struct
{
	char		*fxb_chars;	/* pointer to allocated space */
	unsigned	fxb_max;	/* size of allocated space */
	unsigned	fxb_rcnt;	/* number of characters read */
	unsigned	fxb_wcnt;	/* number of characters written */
/* public: */
/*
 * Initialize a Flexbuf.
 */
#define			flexnew(f)	((f)->fxb_rcnt = (f)->fxb_wcnt = (f)->fxb_max = 0)
/*
 * Reset a Flexbuf by clearing its contents, but without freeing the
 * dynamically allocated space.
 */
#define			flexclear(f)	((f)->fxb_rcnt = (f)->fxb_wcnt = 0)
/*
 * Test whether a Flexbuf is empty.
 */
#define			flexempty(f)	((f)->fxb_rcnt >= (f)->fxb_wcnt)
/*
 * Remove last character from a Flexbuf.
 */
#define			flexrmchar(f)	do { if (!flexempty(f)) \
						--(f)->fxb_wcnt; \
					   } while(0);
/*
 * Return number of characters in a Flexbuf.
 */
#define			flexlen(f)	(flexempty(f) ? 0 : \
					 (f)->fxb_wcnt - (f)->fxb_rcnt)
}
Flexbuf;

/*
 * Structure used to hold all information about a "buffer" -
 * i.e. the representation of a file in memory.
 */
typedef struct buffer {
    Line		*b_line0;	/* ptr to zeroth line of file */
    Line		*b_file;	/* ptr to first line of file */
    Line		*b_lastline;	/* ptr to (n+1)th line of file */

    /*
     * Stuff for line-Undo command
     */
    Line		*b_prevline;	/* The Line passed to the previous call
					 * of move_cursor() for this buffer
					 */
    char		*b_Undotext;	/* What to restore the current line to
					 * if they use the U command. It is an
					 * allocated copy and should be freed.
					 */
    /*
     * All of these are allocated, and should be freed
     * before assigning any new value to them.
     */
    char		*b_filename;	/* file name, if any */
    char		*b_tempfname;	/* name for temporary copy of file */

    unsigned int	b_flags;	/* flags */

    int			b_nwindows;	/* no of windows open on this buffer */

    /*
     * The following only used in mark.c.
     */
    Mark		b_mlist[NMARKS];	/* current marks */
    Mark		b_pcmark;		/* previous context mark */

    /*
     * The following only used in undo.c.
     */
    ChangeData		*b_change;

} Buffer;

/*
 * Definitions for the "flags" field of a buffer.
 */
#define	FL_MODIFIED	0x1
#define	FL_READONLY	0x2
#define	is_modified(b)	((b)->b_flags & FL_MODIFIED)
#define	is_readonly(b)	(Pb(P_readonly) || ((b)->b_flags & FL_READONLY))

/*
 * Utility functions that work on Buffers
 */
#define b_last_line_of(b)	((b)->b_lastline->l_prev)

/*
 * Structure used to hold information about a "window" -
 * this is intimately associated with the Buffer structure.
 */
typedef struct xviwin {
    Posn		*w_cursor;	/* cursor's position in buffer */

    Buffer		*w_buffer;	/* buffer we are a window into */

    Line		*w_topline;	/* line at top of screen */
    Line		*w_botline;	/* line below bottom of screen */

    VirtScr		*w_vs;		/* virtual screen for window */

    unsigned		w_nrows;	/* number of rows in window */
    unsigned		w_ncols;	/* number of columns in window */
    unsigned		w_winpos;	/* row of top line of window */
    unsigned		w_cmdline;	/* row of window command line */

    /*
     * Allocated within screen.c.
     */
    Flexbuf		w_statusline;	/* text on status line */
    /* public: */
#   define		sline_text(w)	(flexgetstr(&(w)->w_statusline))

    /*
     * These elements are related to the cursor's position in the window.
     */
    int			w_row, w_col;	/* cursor's position in window */

    int			w_virtcol;	/* column number of the file's actual */
					/* line, as opposed to the column */
					/* number we're at on the screen. */
					/* This makes a difference on lines */
					/* which span more than one screen */
					/* line. */

    int			w_curswant;	/* The column we'd like to be at. */
					/* This is used to try to stay in */
					/* the same column through up/down */
					/* cursor motions. */

    bool_t		w_set_want_col;	/* If set, then update w_curswant */
					/* the next time through cursupdate() */
					/* to the current virtual column */

    int			w_c_line_size;	/* current size of cursor line */

    /*
     * The following only used in windows.c.
     */
    struct xviwin	*w_last;	/* first and last pointers */
    struct xviwin	*w_next;

    Cmd			*w_cmd;		/* info about current command */

} Xviwin;

/*
 * Status line glitch handling.
 *
 * SPARE_COLS is the number of columns which are not used at the
 * end of the status line; this is to prevent wrapping on this line,
 * as this can do strange things to some terminals.
 */
#define	SPARE_COLS	1

typedef struct tag {
    struct tag	*t_next;
    char	*t_name;
    char	*t_file;
    char	*t_locator;
} TAG;

/*
 * Values returned by inc() & dec().
 */
enum mvtype {
    mv_NOMOVE = -1,	/* at beginning or end of buffer */
    mv_SAMELINE = 0,	/* still within same line */
    mv_CHLINE = 1,	/* changed to different line */
    mv_EOL = 2		/* in same line, at terminating '\0' */
};

/*
 * Number of input characters since the last buffer preservation.
 */
extern volatile int	keystrokes;

/*
 * Minimum number of keystrokes after which we do an automatic
 * preservation of all modified buffers.
 */
#define PSVKEYS		60

/*
 * Exceptional return values for get_file().
 */
#define gf_NEWFILE	((long)-1)	/* no such file */
#define gf_CANTOPEN	((long)-2)	/* error opening file */
#define gf_IOERR	((long)-3)	/* error reading from file */
#define gf_NOMEM	((long)-4)	/* not enough memory */
#define gf_INTERRUPTED	((long)-5)	/* Aborted with ^C */


/***************************************************************
 *                                                             *
 * SECTION 7: MISCELLANEOUS MACROS                             *
 *                                                             *
 ***************************************************************/


/***************************************************************
 *                                                             *
 * SECTION 8: XVI-LOCAL HEADER FILES                           *
 *                                                             *
 * Various subsidiary header files with definitions relating   *
 * to particular areas of the editor (or its environment).     *
 * Note that these header files may use definitions in this    *
 * file, so are included after all types are defined.          *
 *                                                             *
 ***************************************************************/

#include "ascii.h"
#include "param.h"
#include "ptrfunc.h"


/***************************************************************
 *                                                             *
 * SECTION 9: SYSTEM-SPECIFIC HEADER FILES                     *
 *                                                             *
 ***************************************************************/

/*
 * Include file for system interface module.
 * We must have one of these.
 */

#ifdef	ATARI
#   include "tos.h"
#   define	GOT_OS
#endif

#ifdef	UNIX
#   include "unix.h"
#   define	GOT_OS
#endif

#ifdef	OS2
    /*
     * Microsoft's wonderful compiler defines MSDOS, even when
     * we're compiling for OS/2, but it doesn't define OS2.
     * Ingenious, eh?
     */
#   undef MSDOS
#   include "os2vio.h"
#   define	GOT_OS
#endif

#ifdef	MSDOS
#   include "msdos.h"
#   define	GOT_OS
#endif

#ifdef	QNX
#   include "qnx.h"
#   define	GOT_OS
#endif

#ifndef	GOT_OS
    no system-specific include file found
#endif


/***************************************************************
 *                                                             *
 * SECTION 10: GLOBAL VARIABLE DECLARATIONS                    *
 *                                                             *
 ***************************************************************/

/*
 * Miscellaneous global vars.
 */
extern	Xviwin		*curwin;	/* current window */
extern	Buffer		*curbuf;	/* current buffer */

/*
 * Set curbuf with this macro, ensuring that curbuf follows it.
 * Be careful when it is set to NULL, and to use (w) only once as it may
 * be a function call or have side-effects.
 */
#define set_curwin(w)	do { \
			    curwin=(w); \
			    curbuf = curwin != NULL ? curwin->w_buffer : NULL; \
			} while(0)

extern	int		indentchars;	/* number of chars of auto-indentation
					 * on current line */
extern	int		next_indent;	/* auto-indent to apply to next line */
extern	bool_t		literal_next;	/* Are we in the middle of a ^V ? */
extern	char		*altfilename;	/* name of current alternate file */
extern	char		Version[];	/* version string for :ve command */

/*
 * This flag is set when a keyboard interrupt is received.
 */
extern volatile unsigned char kbdintr;

/*
 * This input character sends a keyboard interrupt, usually CTRL('C').
 */
extern char kbdintr_ch;

/*
 * This one indicates whether we should display the "Interrupted"
 * message.
 */
extern	bool_t		imessage;

/*
 * This flag is set when a keyboard-generated suspension request
 * has been generated, i.e. (on a UNIX system) the user hit ^Z.
 */
extern volatile bool_t SIG_suspend_request;

/*
 * This flag is set as a result of the user having disconnected.
 */
extern volatile bool_t SIG_user_disconnected;

/*
 * This flag is set when a termination signal is received.
 */
extern volatile bool_t SIG_terminate;

/*
 * This variable (defined in main.c) is a bitmap which controls the
 * verbosity of screen output. The meanings of the individual bits
 * are:
 *
 *	e_CHARUPDATE means it's OK to update individual characters in
 *	any window.
 *
 *	e_SCROLL means it's OK to scroll any area of the screen up or
 *	down.
 *
 *	e_REPORT means it's OK for report() to report the number of
 *	lines inserted or deleted.
 *
 *	e_SHOWINFO means it's OK for show_file_info() to display file
 *	information for any buffer.
 *
 *	e_BEEP: not implemented yet.
 *
 *	e_REGERR means it's OK for functions in search.c to display
 *	messages which may have resulted from an invalid regular
 *	expression string.
 *
 *	e_NOMATCH means it's OK for functions in search.c to complain
 *	if they fail to match a regular expression at least once.
 *
 * If we're reading an input sequence & e_CHARUPDATE & e_SCROLL are
 * turned off, no screen updating will be done until an ESC is
 * received.
 */
extern	unsigned	echo;

#define	e_CHARUPDATE	1
#define	e_SCROLL	2
#define	e_REPORT	4
#define	e_SHOWINFO	010
#define	e_BEEP		020
#define	e_REGERR	040
#define	e_NOMATCH	0100
#define	e_ALLOCFAIL	0200

#define e_ANY		0xffff

/*
 * Variables which should not be global, but are temporarily so
 * because of code rearrangements. This will be fixed up later.
 */
extern	int		operator;
#define	NOP		'\0'		/* no pending operation */

extern	int	cur_yp_name;

/*
 * Redo buffer. This should really be part of a structure created
 * when the editor is invoked, but it's just a global for now.
 */
struct redo {
    enum {
	r_insert,
	r_replace1,
	r_normal
    }		r_mode;
    Flexbuf	r_fb;
};

extern	struct redo	Redo;

/***************************************************************
 *                                                             *
 * SECTION 11: FUNCTION DECLARATIONS                           *
 *                                                             *
 ***************************************************************/

/*
 * Declarations of all the routines exported from the various .c files.
 */

/*
 * alloc.c
 */
extern	char	out_of_memory[];
extern	void	*alloc P((size_t));
extern	void	*re_alloc P((void *, size_t));
extern	void	*clr_alloc P((size_t, size_t));
extern	char	*strsave P((const char *));
extern	Line	*newline P((int));
extern	Line	*snewline P((char *));
extern	bool_t	lnresize P((Line *, unsigned));
extern	bool_t	bufempty P((void));
extern	bool_t	buf1line P((Buffer *));
extern	bool_t	endofline P((Posn *));
extern	bool_t	grow_line P((Line *, int));
extern	void	throw P((Line *));

/*
 * altstack.c
 */
extern	char	*alt_file_name P((void));
extern	void	push_alternate P((char *, long));
extern	char	*pop_alternate P((long *));

/*
 * ascii.c
 */
extern unsigned vischar P((int, char **, int));

/*
 * buffers.c
 */
extern	Buffer	*new_buffer P((void));
extern	void	free_buffer P((Buffer *));
extern	bool_t	clear_buffer P((Buffer *));
extern	int	nbuffers;

/*
 * cmdline.c
 */
extern	bool_t	should_fexpand P((char *));
extern	bool_t	exCommand P((char *));
extern	void	wait_return P((void));

/*
 * cmdmode.c
 */
extern	void	cmd_init P((int));
extern	Cmd_State
		cmd_input P((int));
extern	char	*get_cmd P((void));
extern	int	get_pos P((void));

/*
 * vi_cmds.c
 */
extern	void	do_cmd P((Cmd *));
extern	void	do_page P((Cmd *));
extern	void	do_scroll P((Cmd *));
extern	void	do_z P((Cmd *));
extern	void	do_x P((Cmd *));
extern	void	do_rchar P((Cmd *));
extern	void	do_ins P((Cmd *));

/*
 * cursor.c
 */
extern	void	cursupdate P((void));

/*
 * defscr.c
 */
extern	void	defscr_main P((int, char **));

/*
 * dispmode.c
 */
extern	void	disp_init P((char *(*) P((void)), int, bool_t));
extern	bool_t	disp_screen P((int));
extern	void	prompt P((char *));

/*
 * edit.c
 */
extern	bool_t	i_proc P((int));
extern	bool_t	r_proc P((int));
extern	void	startinsert P((bool_t, int));
extern	void	startreplace P((Cmd *));
extern	char	*mkstr P((int));

/*
 * events.c
 */
extern	xvResponse	*xvi_handle_event P((xvEvent *));
extern	bool_t		interactive;

/*
 * ex_cmds1.c
 */
extern	void	exQuit P((bool_t));
extern	void	exSplitWindow P((void));
extern	bool_t	exNewBuffer P((char *, int));
extern	bool_t	exCloseWindow P((bool_t));
extern	bool_t	exXit P((void));
extern	bool_t	exEditFile P((bool_t, char *));
extern	bool_t	exArgs P((void));
extern	bool_t	exNext P((int, char **, bool_t));
extern	bool_t	exRewind P((bool_t));
extern	bool_t	exAppendToFile P((char *, Line *, Line *, bool_t));
extern	bool_t	exWriteToFile P((char *, Line *, Line *, bool_t));
extern	void	exWQ P((char *, bool_t));
extern	bool_t	exReadFile P((char *, Line *));
extern	void	exEditAlternateFile P((void));
extern	void	exShowFileStatus P((char *));
extern	char	nowrtmsg[];

/*
 * ex_cmds2.c
 */
extern	void	exInteractiveShell P((bool_t));
extern	bool_t	exShellCommand P((char *));
extern	void	exSuspend P((bool_t));
extern	void	exEquals P((Line *));
extern	void	exHelp P((void));
extern	bool_t	exSource P((char *));
extern	char	*exChangeDirectory P((char *));
extern	bool_t	exLineOperation P((int, Line *, Line *, Line *));
extern	bool_t	exJoin P((Line *, Line *, bool_t));

/*
 * fileio.c
 */
extern	bool_t	set_format P((Paramval, bool_t));
extern	long	get_file P((char *, Line **, Line **, char *,
							char *));
extern	bool_t	appendit P((char *, Line *, Line *, bool_t));
extern	bool_t	writeit P((char *, Line *, Line *, bool_t));
extern	bool_t	put_file P((FILE *, Line *, Line *,
				unsigned long *, unsigned long *));

/*
 * find.c
 */
extern	Posn	*searchc P((int, int, int, int));
extern	Posn	*crepsearch P((Buffer *, int, int));
extern	Posn	*showmatch P((void));
extern	Posn	*fwd_word P((Posn *, int, bool_t));
extern	Posn	*bck_word P((Posn *, int, bool_t));
extern	Posn	*end_word P((Posn *, int, bool_t));
extern	Posn	*xvDoSearch P((char *, int));
extern	Posn	*xvLocateTextObject P((Posn *, int, int));

/*
 * flexbuf.c
 */
extern	bool_t	flexaddch P((Flexbuf *, int));
extern	bool_t	flexrm P((Flexbuf *, int, int));
extern	bool_t	flexinsch P((Flexbuf *, int, int));
extern	bool_t	flexinsstr P((Flexbuf *, int, char *));
extern	char	*flexgetstr P((Flexbuf *));
extern	char	*flexdetach P((Flexbuf *));
extern	int	flexpopch P((Flexbuf *));
extern	void	flexdelete P((Flexbuf *));
extern	bool_t	vformat P((Flexbuf *, char *, va_list));
extern	bool_t	lformat P((Flexbuf *, char *, ...));

/*
 * map.c
 */
extern	void	stuff P((char *, ...));
extern	void	stuff_to_map P((char *));
extern	void	unstuff P((void));
extern	int	map_getc P((void));
extern	void	map_char P((int));
extern	void	map_timeout P((void));
extern	bool_t	map_waiting P((void));
extern	int	mapped_char P((int));
extern	bool_t	xvi_map P((char *, bool_t));
extern	void	xvi_keymap P((char *, char *));
extern	bool_t	xvi_unmap P((int, char **, bool_t));

/*
 * mark.c
 */
extern	void	init_marks P((Buffer *));
extern	bool_t	setmark P((int, Buffer *, Posn *));
extern	void	setpcmark P((void));
extern	Posn	*getmark P((int, Buffer *));
extern	void	clrmark P((Line *, Buffer *));
extern	void	restoremarks P((Line *, Buffer *));

/*
 * misccmds.c
 */
extern	bool_t	openfwd P((Posn *, bool_t));
extern	bool_t	openbwd P((void));
extern	long	cntllines P((Line *, Line *));
extern	long	cntplines P((Line *, Line *));
extern	long	plines P((Line *));
extern	Line	*gotoline P((Buffer *, unsigned long));
extern	int	get_indent P((Line *));
extern	int	set_indent P((Line *, int));
extern	void	tabinout P((int, Line *, Line *, Cmd *));
extern	void	makeargv P((char *, int *, char ***, char *));
extern	void	xvConvertWhiteSpace P((char *));
extern	bool_t	xvJoinLine P((Line *, bool_t));
extern	bool_t	xvChangesNotSaved P((void));
extern	void	xvAutoWriteAll P((void));
extern	void	xvAutoWrite P((void));

/*
 * mouse.c
 */
extern	void	mouseclick P((int, int));
extern	void	mousedrag P((int, int, int, int));
extern	void	mousemove P((int));

/*
 * movement.c
 */
extern	int	shiftdown P((unsigned));
extern	int	shiftup P((unsigned));
extern	void	scrolldown P((unsigned));
extern	void	scrollup P((unsigned));
extern	bool_t	xvMoveUp P((Posn *, long, bool_t));
extern	bool_t	xvMoveDown P((Posn *, long, bool_t));
extern	bool_t	one_left P((bool_t));
extern	bool_t	xvMoveLeft P((Posn *, bool_t));
extern	bool_t	one_right P((bool_t));
extern	bool_t	xvMoveRight P((Posn *, bool_t));
extern	void	begin_line P((bool_t));
extern	void	xvSetPosnToStartOfLine P((Posn *, bool_t));
extern	void	xvMoveToColumn P((Posn *, int));
extern	void	xvMoveToLineNumber P((long));
extern	void	move_cursor P((Line *, int));
extern	void	move_window_to_cursor P((void));
extern	void	move_cursor_to_window P((void));

/*
 * normal.c
 */
extern	bool_t	normal P((int));
extern	bool_t	xvProcessSearch P((int, char *));
extern	void	xvInitialiseCmd P((Cmd *));
extern	void	do_operator P((Cmd *));
extern	void	do_quote P((Cmd *));
extern	void	do_badcmd P((Cmd *));

/*
 * vi_ops.c
 */
extern	void	xvOpShift P((Cmd *));
extern	void	xvOpDelete P((Cmd *));
extern	void	xvOpChange P((Cmd *));
extern	void	xvOpYank P((Cmd *));

/*
 * param.c
 */
extern	void	init_params P((void));
extern	void	exSet P((int, char **));
extern	void	set_param P((int, ...));
extern	int	xv_strtoi P((char **));

/*
 * pipe.c
 */
extern	void	specify_pipe_range P((Line *, Line *));
extern	bool_t	do_pipe P((char *));
extern	bool_t	xvWriteToCommand P((char *, Line *, Line *));
extern	bool_t	xvReadFromCommand P((char *, Line *));

/*
 * preserve.c
 */
extern	bool_t	preservebuf P((void));
extern	void	unpreserve P((void));
extern	bool_t	exPreserveAllBuffers P((void));

/*
 * ptrfunc.c
 */
extern	enum mvtype	inc P((Posn *));
extern	enum mvtype	dec P((Posn *));
extern	void	pswap P((Posn *, Posn *));
extern	bool_t	lt P((Posn *, Posn *));
extern	bool_t	eq P((Posn *, Posn *));

/*
 * screen.c
 */
extern	void	updateline P((bool_t));
extern	void	update_sline P((void));
extern	void	update_cline P((void));
extern	void	redraw_window P((bool_t));
extern	void	redraw_all P((bool_t));
extern	void	s_ins P((int, int));
extern	void	s_del P((int, int));
extern	void	s_inschar P((int));
extern	void	wind_goto P((void));
extern	void	gotocmd P((bool_t));
extern	void	beep P((void));

/*
 * search.c
 */
extern	Posn	*search P((Line *, int, int, char **));
extern	Posn	*xvFindPattern P((Posn *, char *, int, bool_t));
extern	Line	*linesearch P((Line *, int, char **));
extern	bool_t	exGlobal P((Line *, Line *, char *, bool_t));
extern	long	exSubstitute P((Line *, Line *, char *));
extern	long	exAmpersand P((Line *, Line *, char *));
extern	long	exTilde P((Line *, Line *, char *));

/*
 * signal.c
 */
extern	void	catch_signals P((void));

/*
 * startup.c
 */
extern	Xviwin	*xvi_startup P((VirtScr *, int, char **));
extern	void	startup_error P((char *));

/*
 * status.c
 */
extern	void	init_sline P((void));
extern	void	show_message P((char *, ...));
extern	void	show_error P((char *, ...));
extern	void	show_file_info P((bool_t));
extern	void	info_update P((void));

/*
 * tags.c
 */
extern	void	tagInit P((void));
extern	void	tagword P((void));
extern	bool_t	exTag P((char *, bool_t, bool_t, bool_t));
extern	TAG	*tagLookup P((char *, int *, int *));
extern	bool_t	tagSetParam P((Paramval, bool_t));

/*
 * targets.c
 */
extern	void	do_target P((Cmd *));
extern	void	do_left_right P((Cmd *));
extern	void	do_csearch P((Cmd *));
extern	void	do_word P((Cmd *));
extern	void	do_HLM P((Cmd *));

/*
 * undo.c
 */
extern	void	init_undo P((Buffer *));
extern	void	free_undo P((Buffer *));
extern	bool_t	start_command P((Cmd *));
extern	void	end_command P((void));
extern	void	replchars P((Line *, int, int, char *));
extern	void	repllines P((Line *, long, Line *));
extern	void	replbuffer P((Line *));
extern	void	undo P((void));
extern	void	undoline P((void));

/*
 * update.c
 */
extern	void	xvUpdateScr P((int, int));
extern	void	xvMarkDirty P((int));
extern	void	xvClearLine P((unsigned));
extern	void	xvClear P(());

/*
 * virtscr.c
 */
extern	bool_t	vs_init P((VirtScr *));
extern	bool_t	vs_resize P((VirtScr *, int, int));
extern	void	vs_free P((VirtScr *));
extern	int	xv_decode_colour P((VirtScr *, int, char *));

/*
 * windows.c
 */
extern	Xviwin	*xvInitWindow P((VirtScr *));
extern	Xviwin	*xvOpenWindow P((int));
extern	Xviwin	*xvCloseWindow P((void));
extern	void	xvMapWindowOntoBuffer P((Xviwin *, Buffer *));
extern	void	xvUnMapWindow P((void));
extern	Xviwin	*xvNextWindow P((Xviwin *));
extern	Xviwin	*xvNextDisplayedWindow P((Xviwin *));
extern	Xviwin	*xvFindWindowByName P((Xviwin *, char *));
extern	void	xvEqualiseWindows P((int));
extern	void	xvResizeWindow P((int));
extern	void	xvAdjustWindows P((VirtScr *));
extern	int	xvMoveStatusLine P((int));
extern	void	xvUseWindow P((void));
extern	void	xvUpdateAllBufferWindows P((void));
extern	bool_t	xvCanSplit P((void));

/*
 * yankput.c
 */
extern	void	init_yankput P((void));
extern	bool_t	do_yank P((Posn *, Posn *, bool_t, int));
extern	bool_t	yank_str P((int, char *, bool_t));
extern	void	do_put P((Posn *, int, int, Cmd *));
extern	void	yp_stuff_input P((int, bool_t, bool_t));
extern	void	yp_push_deleted P((void));
