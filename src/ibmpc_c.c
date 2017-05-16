/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    ibmpc_c.c
* module function:
    C part of terminal interface module for IBM PC compatibles
    running MS-DOS.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

/*
 * Screen dimensions, defined here so they'll go in the C default data
 * segment.
 */
unsigned int	Rows;
unsigned int	Columns;

/*
 * Declare this here so we can access the colour elements.
 */
extern	VirtScr		defscr;

/*
 * IBM-compatible PC's have a default typeahead buffer which is only
 * big enough for 16 characters, & some 8088-based PC's are so
 * unbelievably slow that xvi can't handle more than about 2
 * characters a second. So we do some input buffering here to
 * alleviate the problem.
 */

static	char	    kbuf[16];
static	char	    *kbrp = kbuf;
static	char	    *kbwp = kbuf;
static	unsigned    kbcount;

static void near
kbfill()
{
    register int	c;

    while (kbcount < sizeof kbuf && (c = getchnw()) >= 0) {
	kbcount++;
	*kbwp = c;
	if (kbwp++ >= &kbuf[sizeof kbuf - 1])
	    kbwp = kbuf;
    }
}

/*
 * Return an input character. This is only ever called when there is a
 * keypress already in the input buffer, waiting to be read.
 */
static int near
kbget()
{
    unsigned char c;

    if (kbcount <= 0) return(EOF);	/* Should never happen */
    --kbcount;
    c = *kbrp;
    if (kbrp++ >= &kbuf[sizeof kbuf - 1])
	kbrp = kbuf;
    return c;
}

/*
 * Convert milliseconds to clock ticks.
 */
#if CLK_TCK > 1000
#		define	MS2CLK(m)	((long)(m) * (CLK_TCK / 1000))
#else
#	if CLK_TCK < 1000
#		define	MS2CLK(m)	((long)(m) / (1000 / CLK_TCK))
#	else
#		define	MS2CLK(m)	(m)
#	endif
#endif

/*
 * inchar() - get a character from the keyboard
 */
int
inchar(long mstimeout)
{
    clock_t		stoptime;
    /*
     * Function keys map to #2 to #9 and #0, for which we return # and put
     * the other char into pending_char to return at the next call.
     */
    static	char	pending_char = 0;
    register	int	c;

    if (pending_char) {
	c = pending_char;
	pending_char = 0;
	return(c);
    }

    if (kbcount == 0) {
	flush_output();
	if (mstimeout != 0) {
	    stoptime = clock() + MS2CLK(mstimeout);
	}
	kbfill();
    }
    for (;;) {
	if (kbcount == 0) {
	    unsigned	prevbstate;
	    unsigned	prevx = 0, prevy = 0; /* =0 only shuts compilers up */
	    bool_t	isdrag;

	    if (State == NORMAL) {
		showmouse();
		prevbstate = 0;
	    }
	    for (; kbcount == 0; kbfill()) {
		/*
		 * Time out if we have to.
		 */
		if (mstimeout != 0 && clock() > stoptime) {
		    break;
		}
		if (kbdintr) {
		    return EOF;
		}
		if (State == NORMAL) {
		    unsigned	buttonstate;
		    unsigned	mousex,
				mousey;

		    /*
		     * If there's no keyboard input waiting to be
		     * read, watch out for mouse events. We don't do
		     * this if we're in insert or command line mode.
		     */

		    buttonstate = mousestatus(&mousex, &mousey) & 7;
		    mousex /= 8;
		    mousey /= 8;
		    if (prevbstate == 0) {
			isdrag = FALSE;
		    } else {
			if (buttonstate) {
			    /*
			     * If a button is being held down, & the
			     * position has changed, this is a mouse
			     * drag event.
			     */
			    if (mousex != prevx || mousey != prevy) {
				hidemouse();
				mousedrag(prevy, mousey, prevx, mousex);
				showmouse();
				isdrag = TRUE;
			    }
			} else {
			    if (!isdrag) {
				/*
				 * They've pressed & released a button
				 * without moving the mouse.
				 */
				hidemouse();
				mouseclick(mousey, mousex);
				showmouse();
			    }
			}
		    }
		    if ((prevbstate = buttonstate) != 0) {
			prevx = mousex;
			prevy = mousey;
		    }
		}
	    }
	    if (State == NORMAL) {
		hidemouse();
	    }
	    if (kbcount == 0) {
		/*
		 * We must have timed out.
		 */
		return EOF;
	    }
	}
	c = kbget();
	/*
	 * On IBM compatible PC's, function keys return '\0' followed
	 * by another character. Check for this, and turn function key
	 * presses into appropriate "normal" characters to do the
	 * right thing in xvi.
	 */
	if (c != '\0') {
	    return(c);
	} else { /* must be a function key press */
	    c = kbget();
	    switch (c) {
	    case 0x3b: return(K_HELP);		/* F1 key */
	    case 0x47: return(K_HOME);		/* home key */
	    case 0x48: return(K_UARROW);	/* up arrow key */
	    case 0x49: return(K_PGUP);		/* page up key */
	    case 0x4b: return(K_LARROW);	/* left arrow key */
	    case 0x4d: return(K_RARROW);	/* right arrow key */
	    case 0x4f: return(K_END);		/* end key */
	    case 0x50: return(K_DARROW);	/* down arrow key */
	    case 0x51: return(K_PGDOWN);	/* page down key */
	    case 0x52: return(K_INSERT);	/* insert key */
	    case 0x53: return(K_DELETE);	/* delete key */

	    case 0x3c: case 0x3d: case 0x3e: case 0x3f: /* F2 to F5 */
	    case 0x40: case 0x41: case 0x42: case 0x43: /* F6 to F9 */
		pending_char = c - 0x3a + '0';
		return('#');
	    case 0x44:				/* F10 key */
		pending_char = '0';
		return('#');
	    /*
	     * default: ignore both characters ...
	     */
	    }
	}
    }
}

#ifdef __ZTC__
#   ifdef DOS386
#	define Z386
#   endif
#endif

#ifndef Z386

/*
 * The routines in ibmpc_a.asm need to call this because they can't
 * invoke C macros directly.
 *
 * Return normal text colour in the al register, status line colour in ah,
 * and Pb(P_vbell) in dx.
 *
 * This will only work with a Microsoft-compatible compiler.
 */
long
cparams()
{
    return ((long) Pb(P_vbell) << 16) |
	   (unsigned short) ((defscr.pv_colours[VSCstatuscolour] << 8) |
		 (unsigned char) defscr.pv_colours[VSCcolour]);
}

#endif	/* not Z386 */
