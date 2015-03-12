/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    qnx.h
* module function:
    Definitions for QNX system interface module.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include <stdlib.h>
#include <time.h>
#include <malloc.h>
#include <lfsys.h>
#include <process.h>

/*
 * The CII compiler wants function pointers to be compared with (void *) 0,
 * which is what NULL is defined as. This is wrong, but just cope with it.
 */
#undef	NOFUNC
#define	NOFUNC	NULL

#ifndef	HELPFILE
#   define	HELPFILE	"/user/local/bin/xvi.help"
#endif

/*
 * These are the buffer sizes we use for reading & writing files.
 */
#define SETVBUF_AVAIL
#define	READBUFSIZ	4096
#define	WRTBUFSIZ	4096

/*
 * Execute a command in a subshell.
 */
#define	call_system(s)	system(s)

/*
 * System-dependent constants.
 */
#define	MAXPATHLEN	79	/* maximum length of full path name */
#define	MAXNAMLEN	16	/* maximum length of file name */
#define	DIRSEPS		"/^"	/*
				 * directory separators within
				 * pathnames
				 */

/*
 * Under QNX, characters with the top bit set are perfectly valid
 * (although not necessarily always what you want to see).
 */
#define	DEF_CCHARS	TRUE
#define	DEF_MCHARS	TRUE

/*
 * Default file format.
 */
#define DEF_TFF		fmt_QNX

/*
 * Size of buffer for file i/o routines.
 * The SETVBUF_AVAIL forces the file i/o routines to
 * use a large buffer for reading and writing, and
 * this results in a large performance improvement.
 */
#define SETVBUF_AVAIL
#define BIGBUF		16384

/*
 * Macros to open files in binary mode.
 */
#define fopenrb(f)	fopen((f),"r")
#define fopenwb(f)	fopen((f),"w")
#define fopenab(f)	fopen((f),"a")

/*
 * Colour handling: QNX attributes.
 * These are defined so as to work on both colour and monochrome screens.
 *
 * The colour word contains the following fields:
 *
 *	eBBB_FFF__uihb
 *
 * where:
 *	e	means enable colour
 *	BBB	is the background colour
 *	FFF	is the foreground colour
 *	u	means underline
 *	i	means inverse
 *	h	means high brightness
 *	b	means blinking
 *
 * The colours that may be represented using the three bits of FFF or
 * BBB are:
 *	0	black		4	red
 *	1	blue		5	magenta
 *	2	green		6	yellow
 *	3	cyan		7	white
 *
 * We always set 'e', sometimes 'h' and never 'u', or 'b'.
 * 'i' is set for colours which want to be inverse in monochrome.
 */
#define	DEF_COLOUR	"0x8700"   /* white on black      */
#define	DEF_SYSCOLOUR	"0x8700"   /* white on black      */
#define	DEF_STCOLOUR	"0xE106"   /* bright cyan on blue */
#define	DEF_ROSCOLOUR	"0xF406"   /* bright white on red */

extern	int		qnx_disp_inited;

/*
 * Declarations for OS-specific routines in qnx.c.
 */
extern	void		sys_exit(int);
extern	bool_t		can_write(char *);
extern	bool_t		exists(char *);
extern	int		call_shell(char *);
extern	void		Wait200ms(void);
extern	void		sys_startv(void);
extern	void		sys_endv(void);
extern	char		*tempfname(char *);
extern	bool_t		sys_pipe P((char *, int (*)(FILE *), long (*)(FILE *)));
extern	char		*fexpand P((char *, bool_t));

/*
 * Declarations for tcap-specific routines in q2tscr.c.
 */
extern	void		tty_endv P((void));

/*
 * This external variable says whether we can do subshells.
 * It is set to TRUE in q2tscr.c, FALSE in q2wscr.c.
 */
extern	bool_t		subshells;
