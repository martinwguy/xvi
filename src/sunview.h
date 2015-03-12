/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    sunview.h
* module function:
    Definitions for terminal interface module for SunView.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

/*
 * Definitions for communications link between front end & back end
 * processes.
 */
#define XYDIGITS	3	/*
				 * Number of hex. digits encoding row
				 * or column position of mouse in
				 * window.
				 */

#define PREFIXCHAR	'\022'	/* control-R */

/*
 * Size of screen.
 */
extern	unsigned int	Rows;
extern	unsigned int	Columns;

/*
 * We can't do these:
 */
#define invis_cursor()		/* invisible cursor (very optional) */
#define vis_cursor()		/* visible cursor (very optional) */

#define can_scroll_area FALSE	/* true if we can set scroll region */
#define scroll_up(s,e,n)
#define scroll_down(s,e,n)
#define can_del_line	TRUE	/* true if we can delete screen lines */
/*
 * tty_linefeed() isn't needed if can_del_line is TRUE.
 */
#define tty_linefeed()
#define can_ins_line	TRUE	/* true if we can insert screen lines */
#define can_inschar	TRUE	/* true if we can insert characters */
#define cost_goto	1	/*
				 * Cost of doing a goto.
				 *
				 * This is a guess, but it's probably
				 * reasonable.
				 */
/*
 * Colour handling (only in monochrome at the moment).
 */
#define DEF_COLOUR	"1"	/* maps to normal text colours */
#define DEF_STCOLOUR	"2"	/* maps to reverse video */
#define DEF_SYSCOLOUR	"0"	/* maps to normal text colours */

extern	void		outchar P((int c));
extern	void		outstr P((char *s));
extern	void		alert P((void));
extern	void		flush_output P((void));
extern	void		set_colour P((int c));
extern	void		tty_goto P((int row, int col));
extern	void		insert_a_line P((void));
extern	void		delete_a_line P((void));
extern	void		inschar P((int));
extern	void		erase_line P((void));
extern	void		erase_display P((void));
extern	void		tty_open P((unsigned int *, unsigned int *));
extern	void		tty_startv P((void));
extern	void		tty_endv P((void));
extern	void		tty_close P((void));
