/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    preserve.c
* module function:
    Buffer preservation routines.

    The exPreserveAllBuffers() routine saves the contents of all
    modified buffers in temporary files. It can be invoked with the
    :preserve command or it may be called by one of the system
    interface modules when at least PSVKEYS keystrokes have been
    read, & at least Pn(P_preservetime) seconds have elapsed
    since the last keystroke. (PSVKEYS is defined in xvi.h.) The
    preservebuf() routine can be used to preserve a single buffer.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

/*
 * Names of values for the P_preserve enumerated parameter.
 */
char	*psv_strings[] =
{
    "unsafe",
    "standard",
    "safe",
    "paranoid",
    NULL
};

/*
 * Open temporary file for current buffer.
 */
static FILE *
psvfile()
{
    register Buffer	*buffer = curbuf;
    FILE		*fp;

    if (buffer->b_tempfname == NULL) {
	char	*fname;

	fname = buffer->b_filename;
	if (fname == NULL)
	    fname = "unnamed";
	buffer->b_tempfname = tempfname(fname);
	if (buffer->b_tempfname == NULL) {
	    show_error(out_of_memory);
	    return(NULL);
	}
    }
    fp = fopenwb(buffer->b_tempfname);
    if (fp == NULL) {
	show_error(buffer->b_tempfname);
	/*
	 * This can happen asynchronously, so put
	 * the cursor back in the right place.
	 */
	wind_goto();
	VSflush(curwin->w_vs);
    }
    return(fp);
}

/*
 * Write contents of current buffer to file & close file. Return TRUE if no
 * errors detected.
 */
static bool_t
putbuf(fp)
register FILE	*fp;
{
    unsigned long	l1, l2;

    if (put_file(fp, (Line *) NULL, (Line *) NULL, &l1, &l2) == FALSE) {
	show_error(curbuf->b_tempfname);
	return(FALSE);
    } else {
	return(TRUE);
    }
}

/*
 * Preserve contents of a single buffer, so that a backup copy is
 * available in case something goes wrong while the file itself is
 * being written.
 *
 * This is controlled by the P_preserve parameter: if it's set to
 * psv_UNSAFE, we just return. If it's psv_STANDARD, to save time, we
 * only preserve the buffer if it doesn't appear to have been
 * preserved recently: otherwise, if it's psv_SAFE or psv_PARANOID, we
 * always preserve it.
 *
 * Return FALSE if an error occurs during preservation, otherwise TRUE.
 */
bool_t
preservebuf()
{
    FILE	*fp;

    if (
	Pn(P_preserve) == psv_UNSAFE
	||
	(
	    Pn(P_preserve) == psv_STANDARD
	    &&
	    /*
	     * If there is a preserve file already ...
	     */
	    curbuf->b_tempfname != NULL
	    &&
	    exists(curbuf->b_tempfname)
	    &&
	    /*
	     * & a preserve appears to have been done recently ...
	     */
	    keystrokes < PSVKEYS
	)
    ) {
	/*
	 * ... don't bother.
	 */
	return(TRUE);
    }

    fp = psvfile();
    if (fp == NULL) {
	return(FALSE);
    }

    return(putbuf(fp));
}

/*
 * This routine is called when we are quitting without writing the
 * current buffer.
 * It removes the preserve file, assuming that preserve != paranoid.
 * It also frees up b_tempfname if it is set.
 */
void
unpreserve()
{
    Buffer *buffer = curbuf;

    if (Pn(P_preserve) != psv_PARANOID && buffer->b_tempfname != NULL) {
	(void) remove(buffer->b_tempfname);
	free(buffer->b_tempfname);
	buffer->b_tempfname = NULL;
    }
}

/*
 * Preserve contents of all modified buffers.
 */
bool_t
exPreserveAllBuffers()
{
    Xviwin		*savecurwin;
    bool_t		psvstatus = TRUE;

    /* Cycle "curwin" through all the open windows, preserving each */
    savecurwin = curwin;
    do {
	if (is_modified(curbuf)) {
	    FILE	*fp;

	    fp = psvfile();
	    if (fp != NULL) {
		if (!putbuf(fp))
		    psvstatus = FALSE;
	    } else {
		psvstatus = FALSE;
	    }
	}
	set_curwin(xvNextWindow(curwin));
    } while (curwin != savecurwin);

    return(psvstatus);
}
