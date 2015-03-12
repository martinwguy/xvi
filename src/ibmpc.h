/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    ibmpc.h
* module function:
    Declarations for terminal interface module for IBM PC
    compatibles running MS-DOS.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include <conio.h>

#define can_ins_line	FALSE
#define can_del_line	FALSE
#define can_scroll_area TRUE
/*
 * tty_linefeed() isn't needed if can_scroll_area is TRUE.
 */
#define tty_linefeed()
#define can_inschar	FALSE
#define inschar(c)
#define cost_goto	0	/* cost of tty_goto() */

#define tty_close()
#define vis_cursor()
#define invis_cursor()
#define insert_a_line()
#define delete_a_line()

/*
 * Colour handling: default screen colours for PC's.
 */
#define DEF_COLOUR	"0x7"	/* white on black */
#define DEF_STCOLOUR	"0x17"	/* white on blue */
#define DEF_SYSCOLOUR	"0x7"	/* white on black */
#define DEF_ROSCOLOUR	"0x4e"	/* yellow on red */
#define DEF_TAGCOLOUR	"0x3"	/* cyan on black */

/*
 * Screen dimensions.
 */
extern unsigned int	Columns;
extern unsigned int	Rows;

/*
 * Declarations for routines in ibmpc_a.asm & ibmpc_c.c:
 */
extern void		alert P((void));
extern void		erase_display P((void));
extern void		erase_line P((void));
extern void		flush_output P((void));
extern void		hidemouse P((void));
extern int		inchar P((long));
extern unsigned		mousestatus P((unsigned *, unsigned *));
extern void		outchar P((int));
extern void		outstr P((char *));
extern void		scroll_down P((unsigned, unsigned, int));
extern void		scroll_up P((unsigned, unsigned, int));
extern void		set_colour P((int));
extern void		showmouse P((void));
extern void		tty_endv P((void));
extern void		tty_goto P((int, int));
extern void		tty_open P((unsigned *, unsigned *));
extern void		tty_startv P((void));
